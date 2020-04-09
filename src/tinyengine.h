#ifndef TINYENGINE_H
#define TINYENGINE_H

void* Tiny_Malloc(u64 Size);
void Tiny_Free(void *Ptr);
u64 Tiny_GetTimerValue();
f64 Tiny_GetTime();

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


#endif // TINYENGINE_H
