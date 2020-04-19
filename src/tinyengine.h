#ifndef TINYENGINE_H
#define TINYENGINE_H

void* Tiny_Malloc(u64 Size);
void Tiny_Free(void *Ptr);
u64 Tiny_GetTimerValue();
f64 Tiny_GetTime();
void Tiny_Yeild(s32 Ms);
s32 Tiny_AddWatch(char* File);
void Tiny_WatchUpdate(s32 *Wd);

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

f64 TimeHigh;
f64 TimeLow;
f64 Maxfps = 72.0f;
f64 FrameTime;
f64 RealTime;
f64 OldRealTime;
f64 OldTime;
f64 ElapsedtTime;
f64 DeltaTime;
s32 FrameCount;
s32 OldFrameCount;
s32 Frames;

void Tiny_FpsWait()
{
	if(DeltaTime < 0.02f)
	{
		Tiny_Yeild(1000);
	}
	TimeLow = TimeHigh;
}

void Tiny_FpsCalc()
{
again:
	TimeHigh = Tiny_GetTime();
	DeltaTime = TimeHigh - TimeLow;
	RealTime += DeltaTime;
	Maxfps = CLAMP (10.0, Maxfps, 1000.0); //60 fps
	if(RealTime - OldRealTime < 1.0/Maxfps)
	{
		Tiny_FpsWait();
		goto again;
	}
	FrameTime = RealTime - OldRealTime;
	ElapsedtTime = RealTime - OldTime;
	Frames = FrameCount - OldFrameCount;	
	if (ElapsedtTime < 0 || Frames < 0)
	{
		OldTime = RealTime;
		OldFrameCount = FrameCount;
		goto wait;
	}
wait:
	OldRealTime = RealTime;
	FrameTime = CLAMP (0.0001, FrameTime, 0.1);
	FrameCount++;
}

#endif // TINYENGINE_H
