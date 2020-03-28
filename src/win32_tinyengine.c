#include "windows.h"

#define VK_USE_PLATFORM_WIN32_KHR
#define TINYENGINE_DEBUG

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>

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

void Pause()
{
	p("Press any key to exit ...");
	char Buf[10];					
	fgets(Buf, 10, stdin); 
	exit(1); 
}

#ifdef TINYENGINE_DEBUG


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

u8 *Tiny_ReadFile(const char *Filename, s32 *Size)
{
	FILE* File = fopen(Filename, "rb");
	ASSERT(File, "File: %s not found!", Filename);

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

void *Tiny_Malloc(u64 Size)
{
	Size += sizeof(u64);
	void *Ptr = HeapAlloc(GetProcessHeap(), 0, Size);
	*(u64*)Ptr = Size;
	return (void*)((u8*)Ptr + sizeof(u64));
}

void Tiny_Free(void *Ptr)
{
	u64 Size = *(u64*)((u8*)Ptr - sizeof(u64));
	HeapFree(GetProcessHeap, 0, Ptr);
}

u64 Tiny_GetTimerValue()
{
	u64 Value;
    QueryPerformanceCounter((LARGE_INTEGER*) &Value);
    return Value;
}

u64 TimerOffset;
f64 Tiny_GetTime()
{
	return (f64)(Tiny_GetTimerValue()-TimerOffset) / 1000000;
}

b32 Exit;
HWND Window;
HINSTANCE HInstance;
vk_entity_t EntIds[1000];

void SurfaceCallback(VkSurfaceKHR* Surface)
{
	VkWin32SurfaceCreateInfoKHR  SurfaceCI;
	SurfaceCI.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
	SurfaceCI.pNext = NULL;
	SurfaceCI.flags = 0;
	SurfaceCI.hinstance = HInstance;
	SurfaceCI.hwnd = Window;

	VK_CHECK(vkCreateWin32SurfaceKHR(Instance, &SurfaceCI, VkAllocators, Surface));
}

static LRESULT CALLBACK
Win32MainWindowCallback(HWND Window, UINT Message, WPARAM WParam, LPARAM LParam)
{
	switch(Message)
	{
		case WM_CLOSE:
			Exit = true;
			break;
	}
	return DefWindowProcW(Window, Message, WParam, LParam);
}

int WINAPI WinMain(HINSTANCE Instance, HINSTANCE PrevInstance, LPSTR CommandLine, int ShowCode)
{
	FILE* File = fopen("./log.txt","w");
	LogSetfp(File);
	
	WNDCLASSW WindowClass = {0};
	WindowClass.style = CS_OWNDC;
	WindowClass.lpfnWndProc = Win32MainWindowCallback;
	WindowClass.hInstance = Instance;
	WindowClass.hCursor = LoadCursor(NULL, IDC_ARROW);
	WindowClass.hIcon = LoadIcon(NULL, IDI_WINLOGO);
	WindowClass.lpszClassName = L"Win32TinyEngineWindowClass"; 
		
	if(RegisterClassW(&WindowClass))
	{
		
		HInstance = Instance;
		Window = CreateWindowExW(0, WindowClass.lpszClassName, L"TinyEngine",
				WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
				CW_USEDEFAULT, CW_USEDEFAULT, 0, 0, Instance, 0);
				
		const char *RequiredExtensions[] =
		{
			VK_KHR_SURFACE_EXTENSION_NAME,
			VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
	#ifdef TINYENGINE_DEBUG
			VK_EXT_DEBUG_REPORT_EXTENSION_NAME
	#endif
		};

		HMODULE VulkanLoader = LoadLibrary("vulkan-1.dll");
		if(!VulkanLoader)
		{
			Fatal("vulkan-1.dll not found!");
			Pause();
		}
		PFN_vkGetInstanceProcAddr ProcAddr = 
		(PFN_vkGetInstanceProcAddr) GetProcAddress(VulkanLoader, "vkGetInstanceProcAddr");
		if(!InitVulkan(&ProcAddr, ArrayCount(RequiredExtensions), RequiredExtensions))
		{
			Fatal("Failed to initialize vulkan runtime!");
			Pause();
		}

		if(ShowWindow(Window, SW_SHOW) == 0)
		{
			while(!Exit)
			{
				MSG Message;
				while(PeekMessageW(&Message, Window, 0, 0, PM_REMOVE))
				{
					TranslateMessage(&Message);
					DispatchMessageW(&Message);
				}
				VkBeginRendering();
				
				vertex_t Vertices[4];
				u32 indeces[6] = {0, 1, 2, 2, 3, 0};
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
				
				VkDrawLightnings(ArrayCount(Vertices), &Vertices[0], ArrayCount(indeces), &indeces[0], &EntIds[4]);

				VkEndRendering();
			}
		}
	}
	else
	{
		Fatal("Failed to register window class.");
	}
	p("Exit");
	return 0;
}
