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


f32 DotProduct (f32 v1[3], f32 v2[3])
{
	return v1[0]*v2[0] + v1[1]*v2[1] + v1[2]*v2[2];
}

void VectorSubtract (f32 veca[3], f32 vecb[3], f32 out[3])
{
	out[0] = veca[0]-vecb[0];
	out[1] = veca[1]-vecb[1];
	out[2] = veca[2]-vecb[2];
}

void VectorAdd (f32 veca[3], f32 vecb[3], f32 out[3])
{
	out[0] = veca[0]+vecb[0];
	out[1] = veca[1]+vecb[1];
	out[2] = veca[2]+vecb[2];
}

void VectorCopy (f32 in[3], f32 out[3])
{
	out[0] = in[0];
	out[1] = in[1];
	out[2] = in[2];
}

void CrossProduct(f32 v1[3], f32 v2[3], f32 cross[3])
{
	cross[0] = v1[1]*v2[2] - v1[2]*v2[1];
	cross[1] = v1[2]*v2[0] - v1[0]*v2[2];
	cross[2] = v1[0]*v2[1] - v1[1]*v2[0];
}

f32 VectorLength(f32 v[3])
{
	return sqrt(DotProduct(v,v));
}

f32 VectorNormalize (f32 v[3])
{
	f32	length, ilength;

	length = sqrt(DotProduct(v,v));

	if (length)
	{
		ilength = 1/length;
		v[0] *= ilength;
		v[1] *= ilength;
		v[2] *= ilength;
	}

	return length;

}

void VectorInverse (f32 v[3])
{
	v[0] = -v[0];
	v[1] = -v[1];
	v[2] = -v[2];
}

void VectorScale (f32 in[3], f32 scale, f32 out[3])
{
	out[0] = in[0]*scale;
	out[1] = in[1]*scale;
	out[2] = in[2]*scale;
}

void PrintVec3(f32 v[3])
{
	p("X:%f Y:%f Z:%f", v[0], v[1], v[2]);
}

void PrintVec4(f32 v[4])
{
	p("X:%f Y:%f Z:%f W:%f", v[0], v[1], v[2], v[3]);
}

void PrintMatrix(f32 matrix[16], char* id)
{
	if(id)
	{
		p("--%s--", id);
		p("Row %d: [ %.6f, %.6f, %.6f, %.6f ]", 0, matrix[0], matrix[1], matrix[2], matrix[3]);
		p("Row %d: [ %.6f, %.6f, %.6f, %.6f ]", 1, matrix[4], matrix[5], matrix[6], matrix[7]);
		p("Row %d: [ %.6f, %.6f, %.6f, %.6f ]", 2, matrix[8], matrix[9], matrix[10], matrix[11]);
		p("Row %d: [ %.6f, %.6f, %.6f, %.6f ]", 3, matrix[12], matrix[13], matrix[14], matrix[15]);
		return;
	}
	p("------");
	p("Row %d: [ %.6f, %.6f, %.6f, %.6f ]", 0, matrix[0], matrix[1], matrix[2], matrix[3]);
	p("Row %d: [ %.6f, %.6f, %.6f, %.6f ]", 1, matrix[4], matrix[5], matrix[6], matrix[7]);
	p("Row %d: [ %.6f, %.6f, %.6f, %.6f ]", 2, matrix[8], matrix[9], matrix[10], matrix[11]);
	p("Row %d: [ %.6f, %.6f, %.6f, %.6f ]", 3, matrix[12], matrix[13], matrix[14], matrix[15]);
}

//if W is zero, you have a problem, so you should check that beforehand. 
//If it is zero, then that means that the object is in the plane of projection
void WorldToScreen(f32 worldp[4], u32 window_width, u32 window_height)
{
	//Vec4Xmat4(worldp, cam.mvp);
	if(worldp[3] != 0)
	{
		worldp[0] /= worldp[3];
		worldp[1] /= worldp[3];
		worldp[2] /= worldp[3];
	}
	worldp[0] = ((worldp[0] / 2.0f) * window_width) + (0.5f * window_width);
	worldp[1] = ((worldp[1] / 2.0f) * window_height) + (0.5f * window_height);
	worldp[2] = 0;

	worldp[0] = CLAMP(0, worldp[0], window_width);
	worldp[1] = CLAMP(0, worldp[1], window_height);

}

void OrthoMatrix(f32 matrix[16], f32 left, f32 right, f32 bottom, f32 top, f32 n, f32 f)
{
	f32 tx = -(right + left) / (right - left);
	f32 ty = (top + bottom) / (top - bottom);
	f32 tz = -(f + n) / (f - n);

	memset(matrix, 0, 16 * sizeof(f32));

	// First column
	matrix[0*4 + 0] = 2.0f / (right-left);

	// Second column
	matrix[1*4 + 1] = -2.0f / (top-bottom);

	// Third column
	matrix[2*4 + 2] = -2.0f / (f-n);

	// Fourth column
	matrix[3*4 + 0] = tx;
	matrix[3*4 + 1] = ty;
	matrix[3*4 + 2] = tz;
	matrix[3*4 + 3] = 1.0f;
}

void Perspective(f32 matrix[16], f32 angle, f32 ratio, f32 n, f32 f)
{
	f32 tan_half_angle = tanf(angle / 2);

	// First column
	matrix[0*4 + 0] = 1.0f / (ratio * tan_half_angle);
	matrix[0*4 + 1] = 0.0f;
	matrix[0*4 + 2] = 0.0f;
	matrix[0*4 + 3] = 0.0f;

	// Second column
	matrix[1*4 + 0] = 0.0f;
	matrix[1*4 + 1] = (1.0f / tan_half_angle);
	matrix[1*4 + 2] = 0.0f;
	matrix[1*4 + 3] = 0.0f;

	// Third column
	matrix[2*4 + 0] = 0.0f;
	matrix[2*4 + 1] = 0.0f;
	matrix[2*4 + 2] = -(f + n) / (f - n);
	matrix[2*4 + 3] = -1.0f;

	// Fourth column
	matrix[3*4 + 0] = 0.0f;
	matrix[3*4 + 1] = 0.0f;
	matrix[3*4 + 2] = -(2 * f * n) / (f - n);
	matrix[3*4 + 3] = 0.0f;
}

/*
===================
MatrixMultiply
====================
*/
void MatrixMultiply(f32 left[16], f32 right[16])
{
	f32 temp[16];
	int column, row, i;

	memcpy(temp, left, 16 * sizeof(f32));
	for(row = 0; row < 4; ++row)
	{
		for(column = 0; column < 4; ++column)
		{
			f32 value = 0.0f;
			for (i = 0; i < 4; ++i)
				value += temp[i*4 + row] * right[column*4 + i];

			left[column * 4 + row] = value;
		}
	}
}

void Vec4Xmat4(f32 v[4], f32 m[16])
{
	f32 copy[4];
	memcpy(copy, v, 4*sizeof(f32));
	v[0] = copy[0]*m[0] + copy[1]*m[4] + copy[2]*m[8] + copy[3]*m[12];
	v[1] = copy[0]*m[1] + copy[1]*m[5] + copy[2]*m[9] + copy[3]*m[13];
	v[2] = copy[0]*m[2] + copy[1]*m[6] + copy[2]*m[10] + copy[3]*m[14];
	v[3] = copy[0]*m[3] + copy[1]*m[7] + copy[2]*m[11] + copy[3]*m[15];
}

/*
=============
RotationMatrix
mat4 =
{
	vec4,
	vec4,
	vec4,
	vec4,
}4
=============
*/
void RotationMatrix(f32 matrix[16], f32 angle, f32 x, f32 y, f32 z)
{
	const f32 c = cosf(angle);
	const f32 s = sinf(angle);

	// First column
	matrix[0*4 + 0] = x * x * (1.0f - c) + c;
	matrix[0*4 + 1] = y * x * (1.0f - c) + z * s;
	matrix[0*4 + 2] = x * z * (1.0f - c) - y * s;
	matrix[0*4 + 3] = 0.0f;

	// Second column
	matrix[1*4 + 0] = x * y * (1.0f - c) - z * s;
	matrix[1*4 + 1] = y * y * (1.0f - c) + c;
	matrix[1*4 + 2] = y * z * (1.0f - c) + x * s;
	matrix[1*4 + 3] = 0.0f;

	// Third column
	matrix[2*4 + 0] = x * z * (1.0f - c) + y * s;
	matrix[2*4 + 1] = y * z * (1.0f - c) - x * s;
	matrix[2*4 + 2] = z * z * (1.0f - c) + c;
	matrix[2*4 + 3] = 0.0f;

	// Fourth column
	matrix[3*4 + 0] = 0.0f;
	matrix[3*4 + 1] = 0.0f;
	matrix[3*4 + 2] = 0.0f;
	matrix[3*4 + 3] = 1.0f;
}


//uses colum major format
//[cam_right.x,    cam_right.y,    cam_right.z,     0]
//[cam_up.x,       cam_up.y,       cam_up.z,        0]
//[cam_forward.x,  cam_forward.y,  cam_forward.z,   0]
//[cam_position.x, cam_position.y, cam_position.z,  1]

void LookAt(f32 matrix[16], f32 eye[3], f32 center[3], f32 up[3])
{
	/* TODO: The negation of of can be spared by swapping the order of
	 *       operands in the following cross products in the right way. */

	f32 f[3];
	VectorSubtract(center, eye, f); //compute forward Z
	VectorNormalize(f);

	f32 s[3];
	CrossProduct(f, up, s); //compute right X vector
	VectorNormalize(s);

	f32 t[3];
	CrossProduct(s, f, t); //compute UP Y

	// First column
	matrix[0*4 + 0] = s[0];
	matrix[0*4 + 1] = t[0];
	matrix[0*4 + 2] = -f[0];
	matrix[0*4 + 3] = 0.0f;

	// Second column
	matrix[1*4 + 0] = s[1];
	matrix[1*4 + 1] = t[1];
	matrix[1*4 + 2] = -f[1];
	matrix[1*4 + 3] = 0.0f;

	// Third column
	matrix[2*4 + 0] = s[2];
	matrix[2*4 + 1] = t[2];
	matrix[2*4 + 2] = -f[2];
	matrix[2*4 + 3] = 0.0f;

	// Fourth column
	matrix[3*4 + 0] = -DotProduct(s, eye);
	matrix[3*4 + 1] = -DotProduct(t, eye);
	matrix[3*4 + 2] = DotProduct(f, eye);
	matrix[3*4 + 3] = 1.0f;
}

/*
=============
TranslationMatrix
=============
*/
void TranslationMatrix(f32 matrix[16], f32 x, f32 y, f32 z)
{
	// First column
	matrix[0*4 + 0] = 1.0f;

	// Second column
	matrix[1*4 + 1] = 1.0f;

	// Third column
	matrix[2*4 + 2] = 1.0f;

	// Fourth column
	matrix[3*4 + 0] = x;
	matrix[3*4 + 1] = y;
	matrix[3*4 + 2] = z;
	matrix[3*4 + 3] = 1.0f;
}

/*
=============
ScaleMatrix
=============
*/
void ScaleMatrix(f32 matrix[16], f32 x, f32 y, f32 z)
{
	// First column
	matrix[0*4 + 0] = x;

	// Second column
	matrix[1*4 + 1] = y;

	// Third column
	matrix[2*4 + 2] = z;

	// Fourth column
	matrix[3*4 + 3] = 1.0f;
}

/*
=============
IdentityMatrix
=============
*/
void IdentityMatrix(f32 matrix[16])
{
	memset(matrix, 0, 16 * sizeof(f32));

	// First column
	matrix[0*4 + 0] = 1.0f;

	// Second column
	matrix[1*4 + 1] = 1.0f;

	// Third column
	matrix[2*4 + 2] = 1.0f;

	// Fourth column
	matrix[3*4 + 3] = 1.0f;
}

#endif // TINYENGINE_H
