#define LINUX_LEAN_AND_MEAN
#define VK_USE_PLATFORM_XLIB_KHR
#define TINYENGINE_DEBUG

#include "tinycc.h"
#include <pthread.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include "tinyengine.h"

#define VK_NO_PROTOTYPES
#include "vulkan_core.h"

#ifndef STB_SPRINTF_IMPLEMENTATION
#define STB_SPRINTF_IMPLEMENTATION
#endif
#include "stb_sprintf.h"

void SurfaceCallback(VkSurfaceKHR* Surface);

enum { LOG_TRACE, LOG_DEBUG, LOG_INFO, LOG_WARN, LOG_ERROR, LOG_FATAL, T_LOG };
b32 LogNewLine = true;
b32 LogExtra = true;
FILE *Logfp;
s32 LogLevel;
s32 LogQuiet;

void LogLog(int level, const char *file, int line, const char *fmt, ...);
void FormatString(char* Buf, char* Str, ...);
void LogSetLevel(s32 Level);
void LogSetfp(FILE *Fp);

//(Kyryl): This log does not care about debug level set.
//useful to do fast print debugging, p(...) is nice and short
//do not leave hanging in production code though.
#define p(...) LogLog(T_LOG, __FILE__, __LINE__, __VA_ARGS__)

#ifdef TINYENGINE_DEBUG

void Pause()
{
	p("Press any key to exit ...");
	char Buf[10];					
	fgets(Buf, 10, stdin); 
	exit(1); 
}

#define ASSERT(condition, message, ...) \
	do { \
		if (! (condition)) \
		{ \
			Fatal(message, ##__VA_ARGS__);\
			FILE *File = fopen(__FILE__, "r");\
			if(File)\
			{\
				int count = 0;\
				char line[1024];\
				LogNewLine = false;\
				LogExtra = false;\
				while(fgets(line, 1024, File)) \
				{\
					count++;\
					if(count >= (__LINE__ - 5) && count <= (__LINE__ + 5))\
					{ Trace("%d %s", count, &line[0]);}\
				}\
			}\
			LogExtra = true;\
			LogNewLine = true;\
			Fatal("Assertion %s failed in, %s line: %d ", #condition, __FILE__, __LINE__);\
			Pause();\
		} \
	} while (false)


#define Trace(...) LogLog(LOG_TRACE, __FILE__, __LINE__, __VA_ARGS__)
#define Debug(...) LogLog(LOG_DEBUG, __FILE__, __LINE__, __VA_ARGS__)
#define Info(...)  LogLog(LOG_INFO,  __FILE__, __LINE__, __VA_ARGS__)
#define Warn(...)  LogLog(LOG_WARN,  __FILE__, __LINE__, __VA_ARGS__)
#define Error(...) LogLog(LOG_ERROR, __FILE__, __LINE__, __VA_ARGS__)
#define Fatal(...) LogLog(LOG_FATAL, __FILE__, __LINE__, __VA_ARGS__)

#else

#define ASSERT(condition, message, ...) {typeof(condition) dummy = condition;};
#define Trace(...)
#define Debug(...)
#define Info(...)
#define Warn(...)
#define Error(...)
#define Fatal(...)

#endif

#include "tiny_vulkan.h"

static const char *level_names[] =
{
	"TRACE", "DEBUG", "INFO", "WARN", "ERROR", "FATAL","TLOG"
};

#ifdef LOG_USE_COLOR
static const char *level_Normalss[] =
{
	"\x1b[94m", "\x1b[36m", "\x1b[32m", "\x1b[33m", "\x1b[31m", "\x1b[35m", "\x1b[94m"
};
#endif

void LogSetfp(FILE *Fp)
{
	Logfp = Fp;
}


void LogSetLevel(s32 Level)
{
	LogLevel = Level;
}

void FormatString(char* Buf, char* Str, ...)
{
	va_list Args;
	va_start(Args, Str);
	stbsp_vsprintf(Buf, Str, Args);
	va_end(Args);
}

void LogLog(s32 Level, const char *File, s32 Line, const char *Fmt, ...)
{
	if (Level < LogLevel)
	{
		return;
	}

	static volatile b32 Lock = 0;

	while(Lock){}; //cheap spinlock

	Lock = 1;

	char Buffer[1024];

	/* Get current time */
	time_t t = time(NULL);
	struct tm lt;
	memcpy(&lt, localtime(&t), sizeof(struct tm));


	/* Log to terminal */
	if (!LogQuiet)
	{
		va_list args;
		if(LogExtra)
		{
			char buf[16] = {0};
			buf[strftime(buf, sizeof(buf), "%H:%M:%S", &lt)] = '\0';
#ifdef LOG_USE_COLOR
			FormatString(buf, "%s %s%-5s\x1b[0m \x1b[90m%s:%d:\x1b[0m ",
					buf, level_Normalss[Level], level_names[Level], File, Line);
			fputs(buf, stderr);
#else
			FormatString(buf, "%s %-5s %s:%d ", buf, level_names[Level], File, Line);
			fputs(buf, stderr);
#endif
		}
		va_start(args, Fmt);
		stbsp_vsprintf(Buffer, Fmt, args);
		//Appends "\n". For special case use. Treat with care.
		if(LogNewLine)
		{
			puts(Buffer);
		}
		else
		{
			fputs(Buffer, stderr);
		}
		va_end(args);
	}

	/* Log to file */
	if (Logfp)
	{
		va_list args;
		if(LogExtra)
		{
			char buf[32] = {0};
			buf[strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &lt)] = '\0';
			FormatString(buf, "%s %-5s %s:%d: ", buf, level_names[Level], File, Line);
			fputs(buf, Logfp);
		}
		va_start(args, Fmt);
		stbsp_vsprintf(Buffer, Fmt, args);
		strcat(Buffer, "\n");
		fputs(Buffer, Logfp);
		va_end(args);
		fflush(Logfp);
	}
	Lock = 0;
}

/*
To avoid naming conflicts and ability to create more than 1 window -
this is a struct.
*/
typedef struct linux_wnd
{ 
	Display* Display;
	Window Window;
	XSetWindowAttributes Attr;
	Drawable RootWindow;
	s32 Screen;
	u32 Width;
	u32 Height; 
	u32 XXyz;
	u32 YXyz;
	u32 BorderWidth;
	u32 Depth;
	u32 ValueMask;
	u32 IsExXyze;
} linux_wnd;

static linux_wnd Wnd;
KeySym KSym;
u64 TimerOffset;
vk_entity_t EntIds[1000];

void SurfaceCallback(VkSurfaceKHR* Surface)
{
	VkXlibSurfaceCreateInfoKHR SurfaceCI;
	SurfaceCI.sType = VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR;
	SurfaceCI.pNext = NULL;
	SurfaceCI.flags = 0;
	SurfaceCI.dpy = Wnd.Display;
	SurfaceCI.window = Wnd.Window;

	VK_CHECK(vkCreateXlibSurfaceKHR(Instance, &SurfaceCI, VkAllocators, Surface));
}

u8 *Tiny_ReadFile(const char *Filename, s32 *Size)
{
	FILE* File = fopen(Filename, "rb");
	ASSERT(File, "Shader file: %s not found!", Filename);

	fseek(File, 0, SEEK_END);
	*Size = ftell(File);
	ASSERT(*Size >= 0, "Empty File.");
	fseek(File, 0, SEEK_SET);

	u8 *Buffer = (u8*) Tiny_Malloc(*Size);

	u32 rc = fread(Buffer, 1, *Size, File);
	ASSERT(rc == (u32)*Size, "Failed to read");
	fclose(File);
	return Buffer;
}

void* Tiny_Malloc(u64 Size)
{
	Size += sizeof(u64);
	void *Ptr = mmap(0, Size, PROT_WRITE | PROT_READ, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	*(u64*)Ptr = Size;
	return (void*)((u8*)Ptr + sizeof(u64));
}

void Tiny_Free(void *Ptr)
{
	u64 Size = *(u64*)((u8*)Ptr - sizeof(u64));
	munmap(Ptr, Size);
}

u64 Tiny_GetTimerValue()
{
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return (u64) tv.tv_sec * (u64) 1000000 + (u64) tv.tv_usec;
}

f64 Tiny_GetTime()
{
	return (f64)(Tiny_GetTimerValue()-TimerOffset) / 1000000;
}

void *EventThread()
{

	XEvent Event;
	while (1) 
	{
		//see https://tronche.com/gui/x/xlib/events/structures.html 
		//NOTE(Kyryl): So the issue was that with validation layers enabled when resizing the window
		//too quickly it triggers and forces assert. Without everything works fine. One way to prevent it is
		//to watch for events, which now work, and stop the renderer until resize has finished. But this looks lame and
		//I don't like it.
		XNextEvent(Wnd.Display, &Event);
		switch(Event.type)
		{
			#ifdef TINYENGINE_DEBUG
			#define DbgEvent Debug
			#else
			#define DbgEvent(...)
			#endif
			case KeyPress:
				DbgEvent("KeyPress");
				XKeyPressedEvent* e = (XKeyPressedEvent*)&(Event);
				KSym = XLookupKeysym(e, 0);
				if(KSym == XK_Escape)
				{
					return NULL;
				}
				break;
			case KeyRelease:
				DbgEvent("KeyRelease");
				break;
			case ButtonPress:
				DbgEvent("ButtonPress");
				break;
			case ButtonRelease:
				DbgEvent("ButtonRelease");
				break;
			case MotionNotify:
				DbgEvent("MotionNotify");
				break;
			case EnterNotify:
				DbgEvent("EnterNotify");
				break;
			case LeaveNotify:
				DbgEvent("LeaveNotify");
				break;
			case FocusIn:
				DbgEvent("FocusIn");
				break;
			case FocusOut:
				DbgEvent("FocusOut");
				break;
			case KeymapNotify:
				DbgEvent("KeymapNotify");
				break;
			case Expose:;
				DbgEvent("Expose");
				break;
			case GraphicsExpose:
				DbgEvent("GraphicsExpose");
				break;
			case NoExpose:
				DbgEvent("NoExpose");
				break;
			case VisibilityNotify:
				DbgEvent("VisibilityNotify");
				break;
			case CreateNotify:
				DbgEvent("CreateNotify");
				break;
			case DestroyNotify:
				DbgEvent("DestroyNotify");
				break;
			case UnmapNotify:
				DbgEvent("UnmapNotify");
				break;
			case MapNotify:
				DbgEvent("MapNotify");
				break;
			case MapRequest:
				DbgEvent("MapRequest");
				break;
			case ReparentNotify:
				DbgEvent("ReparentNotify");
				break;
			case ConfigureNotify: ;
				DbgEvent("ConfigureNotify");
				break;
			case ConfigureRequest: ;
				DbgEvent("ConfigureRequest");
				break;
			case GravityNotify:
				DbgEvent("GravityNotify");
				break;
			case ResizeRequest:
				DbgEvent("ResizeRequest");
				break;
		}
	}
	return NULL;
}

int main(int argc0, char** argv0)
{
	TCCState *s, *s1;
	int ret, opt, n = 0, t = 0, done;
	unsigned start_time = 0;
	const char *first_file;
	int argc;
	char **argv;
	FILE *ppfp = stdout;
	
redo:
	argc = argc0, argv = argv0;	
	s = s1 = tcc_new();

	if(argc0 == 1)
	{
		opt = tcc_set_options(s1, 
		"linux_x11_tinyengine.c -I./shaders -I/usr/X11R6/include -L/usr/X11R6/lib -lX11 -lXext -lm -pthread -ldl -run");
	}
	else
	{
		opt = tcc_parse_args(s, &argc, &argv, 1);
	}
	
	if (n == 0)
	{
		if (opt == OPT_HELP)
			return fputs(help, stdout), 0;
		if (opt == OPT_HELP2)
			return fputs(help2, stdout), 0;
		if (opt == OPT_M32 || opt == OPT_M64)
			tcc_tool_cross(s, argv, opt); /* never returns */
		if (s->verbose)
			printf(version);
		if (opt == OPT_AR)
			return tcc_tool_ar(s, argc, argv);
		if (opt == OPT_V)
			return 0;
		if (opt == OPT_PRINT_DIRS)
		{
			/* initialize search dirs */
			set_environment(s);
			tcc_set_output_type(s, TCC_OUTPUT_MEMORY);
			print_search_dirs(s);
			return 0;
		}

		if (s->nb_files == 0)
			tcc_error("no input files\n");

		if (s->output_type == TCC_OUTPUT_PREPROCESS)
		{
			if (s->outfile && 0!=strcmp("-",s->outfile))
			{
				ppfp = fopen(s->outfile, "w");
				if (!ppfp)
					tcc_error("could not write '%s'", s->outfile);
			}
		}
		else if (s->output_type == TCC_OUTPUT_OBJ && !s->option_r)
		{
			if (s->nb_libraries)
				tcc_error("cannot specify libraries with -c");
			if (s->nb_files > 1 && s->outfile)
				tcc_error("cannot specify output file with -c many files");
		}
		else
		{
			if (s->option_pthread)
				tcc_set_options(s, "-lpthread");
		}

		if (s->do_bench)
			start_time = getclock_ms();
	}

	set_environment(s);
	if (s->output_type == 0)
		s->output_type = TCC_OUTPUT_EXE;
	tcc_set_output_type(s, s->output_type);
	s->ppfp = ppfp;

	if ((s->output_type == TCC_OUTPUT_MEMORY
	        || s->output_type == TCC_OUTPUT_PREPROCESS)
	        && (s->dflag & 16))   /* -dt option */
	{
		if (t)
			s->dflag |= 32;
		s->run_test = ++t;
		if (n)
			--n;
	}

	/* compile or add each files or library */
	first_file = NULL, ret = 0;
	do
	{
		struct filespec *f = s->files[n];
		s->filetype = f->type;
		if (f->type & AFF_TYPE_LIB)
		{
			if (tcc_add_library_err(s, f->name) < 0)
				ret = 1;
		}
		else
		{
			if (1 == s->verbose)
				printf("-> %s\n", f->name);
			if (!first_file)
				first_file = f->name;
			if (tcc_add_file(s, f->name) < 0)
				ret = 1;
		}
		done = ret || ++n >= s->nb_files;
	}
	while (!done && (s->output_type != TCC_OUTPUT_OBJ || s->option_r));

	if (s->run_test)
	{
		t = 0;
	}
	else if (s->output_type == TCC_OUTPUT_PREPROCESS)
	{
		;
	}
	else if (0 == ret)
	{
		if (s->output_type == TCC_OUTPUT_MEMORY)
		{
#ifdef TCC_IS_NATIVE
			ret = tcc_run(s, argc, argv);
#endif
		}
		else
		{
			if (!s->outfile)
				s->outfile = default_outputfile(s, first_file);
			if (tcc_output_file(s, s->outfile))
				ret = 1;
			else if (s->gen_deps)
				gen_makedeps(s, s->outfile, s->deps_outfile);
		}
	}

	if (s->do_bench && done && !(t | ret))
		tcc_print_stats(s, getclock_ms() - start_time);
	tcc_delete(s);
	if (!done)
		goto redo; /* compile more files with -c */
	if (t)
		goto redo; /* run more tests with -dt -run */
	if (ppfp && ppfp != stdout)
		fclose(ppfp);
	return ret;

}

int TinyEngineMain(int argc, char** argv)
{
	FILE* File = fopen("./log.txt","w");
	LogSetfp(File);

	Wnd.Display = XOpenDisplay(getenv("DISPLAY"));
	if (Wnd.Display == NULL)
	{
		Fatal("Cannot open display!");
		exit(1);
	}

	Wnd.Screen = XDefaultScreen(Wnd.Display);

	Wnd.Attr.background_pixel = XBlackPixel(Wnd.Display, Wnd.Screen);
	Wnd.ValueMask |= CWBackPixel;
	Wnd.Depth = DefaultDepth(Wnd.Display, Wnd.Screen);
	Wnd.RootWindow = RootWindow(Wnd.Display, DefaultScreen(Wnd.Display));
	Wnd.Width = 640;
	Wnd.Height = 480;
	Wnd.XXyz = 0;
	Wnd.YXyz = 0;
	Wnd.BorderWidth = 0;
	Wnd.Window = XCreateWindow(Wnd.Display, XRootWindow(Wnd.Display, Wnd.Screen),
			Wnd.XXyz, Wnd.YXyz, Wnd.Width, Wnd.Height,
			Wnd.BorderWidth, Wnd.Depth, InputOutput,
			DefaultVisual(Wnd.Display, Wnd.Screen),
			Wnd.ValueMask, &Wnd.Attr);

	XSelectInput(Wnd.Display, Wnd.Window, 
			ExposureMask | KeyPressMask | KeyReleaseMask | PointerMotionMask |
			ButtonPressMask | ButtonReleaseMask  | StructureNotifyMask | ResizeRequest |
			ButtonPress | ButtonRelease);

	XMapWindow(Wnd.Display, Wnd.Window);
	XInitThreads();

	// Set window title
	XStoreName(Wnd.Display, Wnd.Window, "TinyEngine");
	//This call is crucial because the window size may be changed by window manager.
	//ProcessEvents();
	pthread_t Ithread;
	ASSERT(!pthread_create(&Ithread, NULL, &EventThread, NULL), "pthread: EventThread failed.");

	const char *RequiredExtensions[] =
	{
		VK_KHR_SURFACE_EXTENSION_NAME,
		VK_KHR_XLIB_SURFACE_EXTENSION_NAME,
#ifdef TINYENGINE_DEBUG
		VK_EXT_DEBUG_REPORT_EXTENSION_NAME
#endif
	};

	void* VulkanLoader = dlopen("libvulkan.so.1", RTLD_NOW | RTLD_DEEPBIND);
	PFN_vkGetInstanceProcAddr ProcAddr = dlsym(VulkanLoader, "vkGetInstanceProcAddr");
	if(!InitVulkan(&ProcAddr, ArrayCount(RequiredExtensions), RequiredExtensions))
	{
		Fatal("Failed to initialize vulkan runtime!");
		exit(1);
	}


	TimerOffset = Tiny_GetTimerValue();

	while (KSym != XK_Escape)
	{
		VkBeginRendering();

		//DRAW commands go in between Begin and End respectively.

		vertex_t Vertices[4];
		u32 indeces[6] = {0, 1, 2, 2, 3, 0};

		Vertices[0].Xyz[0] = -0.5f;   //x
		Vertices[0].Xyz[1] = -0.5f;   //y
		Vertices[0].Xyz[2] = 0.0f;    //z
		Vertices[0].Normals[0] = 1.0f; //r
		Vertices[0].Normals[1] = 0.0f; //g
		Vertices[0].Normals[2] = 0.0f; //b
		Vertices[0].UVs[0] = 1.0f;
		Vertices[0].UVs[1] = 0.0f;

		Vertices[1].Xyz[0] = 0.5f;
		Vertices[1].Xyz[1] = -0.5f;
		Vertices[1].Xyz[2] = 0.0f;
		Vertices[1].Normals[0] = 0.0f;
		Vertices[1].Normals[1] = 1.0f;
		Vertices[1].Normals[2] = 0.0f;
		Vertices[1].UVs[0] = 0.0f;
		Vertices[1].UVs[1] = 0.0f;

		Vertices[2].Xyz[0] = 0.5f;
		Vertices[2].Xyz[1] = 0.5f;
		Vertices[2].Xyz[2] = 0.0f;
		Vertices[2].Normals[0] = 0.0f;
		Vertices[2].Normals[1] = 0.0f;
		Vertices[2].Normals[2] = 1.0f;
		Vertices[2].UVs[0] = 0.0f;
		Vertices[2].UVs[1] = 1.0f;

		Vertices[3].Xyz[0] = -0.5f;
		Vertices[3].Xyz[1] = 0.5f;
		Vertices[3].Xyz[2] = 0.0f;
		Vertices[3].Normals[0] = 1.0f;
		Vertices[3].Normals[1] = 1.0f;
		Vertices[3].Normals[2] = 1.0f;
		Vertices[3].UVs[0] = 1.0f;
		Vertices[3].UVs[1] = 1.0f;

		//VkDrawBasic(ArrayCount(Vertices), &Vertices[0], ArrayCount(indeces), &indeces[0], &EntIds[0]);

		vertex_t Line[2];
		Line[0].Xyz[0] = -0.7f;   //x
		Line[0].Xyz[1] = -0.7f;   //y
		Line[0].Xyz[2] = 0.0f;   //z
		Line[0].Normals[0] = 1.0f; //r
		Line[0].Normals[1] = 0.0f; //g
		Line[0].Normals[2] = 0.0f; //b

		Line[1].Xyz[0] = 0.7f;
		Line[1].Xyz[1] = 0.7f;
		Line[1].Xyz[2] = 0.0f;   //z
		Line[1].Normals[0] = 1.0f; //r
		Line[1].Normals[1] = 0.0f; //g
		Line[1].Normals[2] = 0.0f; //b

		//VkDrawLine(ArrayCount(Line), &Line[0], &EntIds[3]);

		//DrawPixCircle(800, 500, 50, 0xFFFFFFFF);

		Vertices[0].Xyz[0] = -1.0f;   //x
		Vertices[0].Xyz[1] = -1.0f;   //y
		Vertices[0].Xyz[2] = 0.0f;   //z
		Vertices[0].UVs[0] = 0.0f;
		Vertices[0].UVs[1] = 0.0f;

		Vertices[1].Xyz[0] = 1.0f;
		Vertices[1].Xyz[1] = -1.0f;
		Vertices[1].Xyz[2] = 0.0f;
		Vertices[1].UVs[0] = 1.0f;
		Vertices[1].UVs[1] = 0.0f;

		Vertices[2].Xyz[0] = 1.0f;
		Vertices[2].Xyz[1] = 1.0f;
		Vertices[2].Xyz[2] = 0.0f;
		Vertices[2].UVs[0] = 1.0f;
		Vertices[2].UVs[1] = 1.0f;

		Vertices[3].Xyz[0] = -1.0f;
		Vertices[3].Xyz[1] = 1.0f;
		Vertices[3].Xyz[2] = 0.0f;
		Vertices[3].UVs[0] = 0.0f;
		Vertices[3].UVs[1] = 1.0f;

		//VkDrawTextured(ArrayCount(Vertices), &Vertices[0], ArrayCount(indeces), &indeces[0], 1, &EntIds[1]);

		VkDrawLightnings(ArrayCount(Vertices), &Vertices[0], ArrayCount(indeces), &indeces[0], &EntIds[4]);

		VkEndRendering();
	}
	p("Exit");
	DeInitVulkan();
	dlclose(VulkanLoader);
	XCloseDisplay(Wnd.Display);
	return 0;
}



#ifdef __TINYC__

int _runmain()
{
	TinyEngineMain(0, NULL);
	p("Exit TinyEngineMain");
	return 0;
}

#endif

