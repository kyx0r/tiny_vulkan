#define LINUX_LEAN_AND_MEAN
#define VK_NO_PROTOTYPES
#define VK_USE_PLATFORM_XLIB_KHR
//#define TINYENGINE_DEBUG

#include "tinycc.h"
#include "tinyengine.h"
#include <pthread.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <sys/inotify.h>
#include "vulkan_core.h"
#include "tiny_vulkan.h"

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

void Tiny_Yeild(s32 Mics)
{
	usleep(Mics);
}


void ProcessEvents()
{
	//see https://tronche.com/gui/x/xlib/events/structures.html 
	//NOTE(Kyryl): So the issue was that with validation layers enabled when resizing the window
	//too quickly it triggers and forces assert. Without everything works fine. One way to prevent it is
	//to watch for events, which now work, and stop the renderer until resize has finished. But this looks lame and
	//I don't like it.
	XEvent Event;
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

void *EventThread()
{
	while (1) 
	{
		ProcessEvents();
		if(KSym == XK_Escape)
		{
			return NULL;
		}
	}
	return NULL;
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
	ProcessEvents();
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
	vk_entity_t EntIds[1000] = {0};


	Tiny_AddWatch("./");


	while (KSym != XK_Escape)
	{
		Tiny_FpsCalc();
		Tiny_WatchUpdate();
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

		VkDrawBasic(ArrayCount(Vertices), &Vertices[0], ArrayCount(indeces), &indeces[0], &EntIds[0]);

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

		DrawPixCircle(800, 600, 50, 0xFFFFFFFF);
		DrawPixCircle(100, 600, 50, 0xFFFFFFFF);

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


		VkDrawTextured(ArrayCount(Vertices), &Vertices[0], ArrayCount(indeces), &indeces[0], 1, &EntIds[1]);

		VkDrawLightnings(300, 200, 1000, 1000, 0.75f, 0.0f, &EntIds[3]);
		if(KSym == XK_Tab)
		{
			EntIds[3].Tag = 2;
		}

		VkEndRendering();
		Tiny_FpsWait();
	}
	DeInitVulkan();
	dlclose(VulkanLoader);
	XCloseDisplay(Wnd.Display);
	return 0;
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
		"linux_x11_tinyengine.c -DHEX_SHADERS -Wall -I./shaders -I/usr/X11R6/include -L/usr/X11R6/lib -lX11 -lXext -lm -pthread -ldl -run");
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

#ifdef __TINYC__

int _runmain()
{
	TinyEngineMain(0, NULL);
	p("Exit TinyEngineMain");
	return 0;
}

#endif

s32 Mfd;
struct timeval MTimeOut;
fd_set MDescriptorSet;
u32 WatchIdx;
s32 FileWIds[10];

void Tiny_AddWatch(char* File)
{
	if(!Mfd)
	{
		Mfd = inotify_init();
		if (Mfd < 0)
		{
			Error("File watch init.");
			return;
		}
		MTimeOut.tv_sec = 0;
		MTimeOut.tv_usec = 0;
		FD_ZERO(&MDescriptorSet);
	}
	s32 Wd = inotify_add_watch(Mfd, File, IN_CLOSE_WRITE | IN_MOVED_TO | IN_CREATE | IN_MOVED_FROM | IN_DELETE);
	if (Wd < 0)
	{
		if(errno == ENOENT)
		{
			Error("File not found: %s", File);
		}
		else
		{
			Error("Filewatch error: %s", strerror(errno));
		}
	}
	FileWIds[WatchIdx++] = Wd;
}

void Tiny_WatchUpdate()
{
	FD_SET(Mfd, &MDescriptorSet);

	s32 ret = select(Mfd + 1, &MDescriptorSet, NULL, NULL, &MTimeOut);
	if(ret < 0)
	{
		Error("Select");
	}
	else if(FD_ISSET(Mfd, &MDescriptorSet))
	{
		s32 len, i = 0;
		char Buff[sizeof(struct inotify_event)*1024];

		len = read (Mfd, Buff, sizeof(struct inotify_event)*1024);

		while (i < len)
		{
			struct inotify_event *pevent = (struct inotify_event *)&Buff[i];
			if(pevent->mask & IN_CLOSE_WRITE)
			{
				if(strstr(pevent->name, ".c") || strstr(pevent->name, ".h"))
				{
					main(1, NULL);
					//break;
				}
			}
			//find that file by id
			for(u32 i = 0; i < WatchIdx; i++)
			{
				//p("Bitch %s ", pevent->name);
				//if(pevent->wd == FileWIds[i])
				//{
				//}
			}
			i += sizeof(struct inotify_event) + pevent->len;
		}
	}
}
