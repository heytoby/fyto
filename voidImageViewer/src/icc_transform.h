/* Fast ICC (matrix/TRC) colour conversion for fyto. See LICENSE.
 *
 * The Windows path used to hand every JPEG that carries an ICC profile to WIC:
 * IWICBitmapFrameDecode -> IWICColorTransform -> CopyPixels. That is a full
 * WIC decode (2-4x slower than libjpeg-turbo) plus a per-pixel colour transform.
 *
 * Almost every camera/phone profile is a matrix/TRC profile: three colorants
 * (rXYZ/gXYZ/bXYZ) and three tone curves (rTRC/gTRC/bTRC). Those convert to
 * sRGB with one 3x3 matrix and two 256-entry tables, so libjpeg-turbo can keep
 * producing the pixels. Profiles that are sRGB-equivalent end up as IDENTITY and
 * cost nothing at all. Anything else (LUT based, CMYK, exotic TRC) is reported
 * as UNSUPPORTED and the caller keeps the WIC fallback.
 */
#ifndef FYTO_ICC_TRANSFORM_H
#define FYTO_ICC_TRANSFORM_H

#include <math.h>
#include <stdlib.h>

#define FYTO_ICC_NONE 0
#define FYTO_ICC_IDENTITY 1
#define FYTO_ICC_MATRIX 2
#define FYTO_ICC_UNSUPPORTED 3

#define FYTO_ICC_ENCODE_SIZE 4096

typedef struct fyto_icc_s
{
	int state;
	float matrix[9];
	float decode[3][256];
	BYTE encode[FYTO_ICC_ENCODE_SIZE + 1];
} fyto_icc_t;

static void fyto_icc_zero(void *p, size_t n)
{
	unsigned char *b = (unsigned char *)p;
	while (n--) *b++ = 0;
}

static DWORD fyto_icc_rd32(const BYTE *p) { return ((DWORD)p[0] << 24) | ((DWORD)p[1] << 16) | ((DWORD)p[2] << 8) | p[3]; }
static WORD fyto_icc_rd16(const BYTE *p) { return (WORD)((p[0] << 8) | p[1]); }
static float fyto_icc_s15f16(const BYTE *p) { return (float)((INT32)fyto_icc_rd32(p)) / 65536.0f; }

/* sRGB transfer functions (IEC 61966-2-1). */
static float fyto_icc_srgb_decode(float v)
{
	return v <= 0.04045f ? v / 12.92f : (float)pow((v + 0.055) / 1.055, 2.4);
}
static float fyto_icc_srgb_encode(float v)
{
	if (v <= 0.0f) return 0.0f;
	if (v >= 1.0f) return 1.0f;
	return v <= 0.0031308f ? v * 12.92f : 1.055f * (float)pow(v, 1.0 / 2.4) - 0.055f;
}

/* D50 XYZ -> linear sRGB (the matrix published in the sRGB ICC profile). */
static const float fyto_icc_d50_to_srgb[9] =
{
	 3.1338561f, -1.6168667f, -0.4906146f,
	-0.9787684f,  1.9161415f,  0.0334540f,
	 0.0719453f, -0.2289914f,  1.4052427f
};

typedef struct fyto_icc_tag_s
{
	DWORD sig, offset, size;
} fyto_icc_tag_t;

static BOOL fyto_icc_find_tag(const BYTE *data, UINT size, DWORD sig, fyto_icc_tag_t *out)
{
	UINT count, i, base = 128;
	if (size < base + 4) return FALSE;
	count = fyto_icc_rd32(data + base);
	if (count > 0x10000 || size < base + 4 + count * 12) return FALSE;
	for (i = 0; i < count; i++)
	{
		const BYTE *entry = data + base + 4 + i * 12;
		if (fyto_icc_rd32(entry) != sig) continue;
		out->sig = sig;
		out->offset = fyto_icc_rd32(entry + 4);
		out->size = fyto_icc_rd32(entry + 8);
		if (out->offset > size || out->size > size - out->offset) return FALSE;
		return TRUE;
	}
	return FALSE;
}

/* Evaluate a curve/parametricCurve tag into a 256 entry lookup table. */
static BOOL fyto_icc_build_curve(const BYTE *data, UINT size, const fyto_icc_tag_t *tag, float out[256])
{
	const BYTE *p;
	int i;
	if (tag->size < 12) return FALSE;
	p = data + tag->offset;
	if (fyto_icc_rd32(p) == 0x63757276) /* 'curv' */
	{
		DWORD count = fyto_icc_rd32(p + 8);
		if (count == 0)
		{
			for (i = 0; i < 256; i++) out[i] = (float)i / 255.0f;
			return TRUE;
		}
		if (count == 1)
		{
			float gamma = (float)fyto_icc_rd16(p + 12) / 256.0f;
			if (gamma <= 0.0f) return FALSE;
			for (i = 0; i < 256; i++) out[i] = (float)pow((double)i / 255.0, gamma);
			return TRUE;
		}
		if (tag->size < 12 + count * 2) return FALSE;
		for (i = 0; i < 256; i++)
		{
			/* Sample the uInt16 table with linear interpolation. */
			double t = (double)i / 255.0 * (count - 1);
			DWORD i0 = (DWORD)t;
			DWORD i1 = i0 + 1 < count ? i0 + 1 : i0;
			double f = t - i0;
			double v0 = fyto_icc_rd16(p + 12 + i0 * 2) / 65535.0;
			double v1 = fyto_icc_rd16(p + 12 + i1 * 2) / 65535.0;
			out[i] = (float)(v0 + (v1 - v0) * f);
		}
		return TRUE;
	}
	if (fyto_icc_rd32(p) == 0x70617261) /* 'para' */
	{
		WORD type = fyto_icc_rd16(p + 8);
		float g = 1.0f, a = 1.0f, b = 0.0f, c = 0.0f, d = 0.0f, e = 0.0f, f = 0.0f;
		switch (type)
		{
			case 0: g = fyto_icc_s15f16(p + 12); break;
			case 1: g = fyto_icc_s15f16(p + 12); a = fyto_icc_s15f16(p + 16); b = fyto_icc_s15f16(p + 20); break;
			case 2: g = fyto_icc_s15f16(p + 12); a = fyto_icc_s15f16(p + 16); b = fyto_icc_s15f16(p + 20); c = fyto_icc_s15f16(p + 24); break;
			case 3: g = fyto_icc_s15f16(p + 12); a = fyto_icc_s15f16(p + 16); b = fyto_icc_s15f16(p + 20); c = fyto_icc_s15f16(p + 24); d = fyto_icc_s15f16(p + 28); break;
			case 4: g = fyto_icc_s15f16(p + 12); a = fyto_icc_s15f16(p + 16); b = fyto_icc_s15f16(p + 20); c = fyto_icc_s15f16(p + 24); d = fyto_icc_s15f16(p + 28); e = fyto_icc_s15f16(p + 32); f = fyto_icc_s15f16(p + 36); break;
			default: return FALSE;
		}
		if (g <= 0.0f || a == 0.0f) return FALSE;
		for (i = 0; i < 256; i++)
		{
			double x = (double)i / 255.0, y;
			switch (type)
			{
				case 0: y = pow(x, g); break;
				case 1: { double t = a * x + b; y = t > 0.0 ? pow(t, g) : 0.0; } break;
				case 2: { double t = a * x + b; y = t > 0.0 ? pow(t, g) + c : c; } break;
				case 3: { double t = a * x + b; y = x >= d ? (t > 0.0 ? pow(t, g) : 0.0) : c * x; } break;
				default: { double t = a * x + b; y = x >= d ? (t > 0.0 ? pow(t, g) : 0.0) + e : c * x + f; } break;
			}
			out[i] = (float)y;
		}
		return TRUE;
	}
	return FALSE;
}

static BOOL fyto_icc_read_xyz(const BYTE *data, const fyto_icc_tag_t *tag, float out[3])
{
	const BYTE *p;
	if (tag->size < 20 || fyto_icc_rd32(data + tag->offset) != 0x58595A20) return FALSE; /* 'XYZ ' */
	p = data + tag->offset + 8;
	out[0] = fyto_icc_s15f16(p);
	out[1] = fyto_icc_s15f16(p + 4);
	out[2] = fyto_icc_s15f16(p + 8);
	return TRUE;
}

/* Convert one pixel through the pipeline; used for the identity probe. */
static void fyto_icc_map(const fyto_icc_t *t, int r, int g, int b, int out[3])
{
	float v[3], w[3];
	int i;
	v[0] = t->decode[0][r];
	v[1] = t->decode[1][g];
	v[2] = t->decode[2][b];
	for (i = 0; i < 3; i++)
		w[i] = t->matrix[i * 3 + 0] * v[0] + t->matrix[i * 3 + 1] * v[1] + t->matrix[i * 3 + 2] * v[2];
	for (i = 0; i < 3; i++)
	{
		float x = w[i];
		int index;
		if (x <= 0.0f) x = 0.0f;
		if (x >= 1.0f) x = 1.0f;
		index = (int)(x * (FYTO_ICC_ENCODE_SIZE - 1) + 0.5f);
		out[i] = t->encode[index];
	}
}

static void fyto_icc_build_encode(fyto_icc_t *t)
{
	int i;
	for (i = 0; i <= FYTO_ICC_ENCODE_SIZE; i++)
	{
		float v = (float)i / (float)FYTO_ICC_ENCODE_SIZE;
		int s = (int)(fyto_icc_srgb_encode(v) * 255.0f + 0.5f);
		t->encode[i] = (BYTE)(s < 0 ? 0 : s > 255 ? 255 : s);
	}
}

/* TRUE when the whole pipeline is within one code value of a straight copy. */
static BOOL fyto_icc_is_identity(const fyto_icc_t *t)
{
	int r, g, b;
	for (r = 0; r < 256; r += 15)
		for (g = 0; g < 256; g += 15)
			for (b = 0; b < 256; b += 15)
			{
				int out[3];
				fyto_icc_map(t, r, g, b, out);
				if (abs(out[0] - r) > 1 || abs(out[1] - g) > 1 || abs(out[2] - b) > 1) return FALSE;
			}
	return TRUE;
}

static void fyto_icc_parse(const BYTE *data, UINT size, fyto_icc_t *out)
{
	fyto_icc_tag_t rtag, gtag, btag, rt, gt, bt;
	float colorant[3][3];
	int i, j;

	fyto_icc_zero(out, sizeof(*out));
	out->state = FYTO_ICC_NONE;
	if (!data || size < 132 || fyto_icc_rd32(data + 36) != 0x61637370) return; /* 'acsp' */
	if (fyto_icc_rd32(data + 16) != 0x52474220) { out->state = FYTO_ICC_UNSUPPORTED; return; } /* not 'RGB ' */
	if (fyto_icc_rd32(data + 20) != 0x58595A20) { out->state = FYTO_ICC_UNSUPPORTED; return; } /* PCS not 'XYZ ' */
	if (!fyto_icc_find_tag(data, size, 0x7258595A, &rtag) ||
		!fyto_icc_find_tag(data, size, 0x6758595A, &gtag) ||
		!fyto_icc_find_tag(data, size, 0x6258595A, &btag) ||
		!fyto_icc_find_tag(data, size, 0x72545243, &rt) ||
		!fyto_icc_find_tag(data, size, 0x67545243, &gt) ||
		!fyto_icc_find_tag(data, size, 0x62545243, &bt))
	{
		out->state = FYTO_ICC_UNSUPPORTED;
		return;
	}
	if (!fyto_icc_read_xyz(data, &rtag, colorant[0]) ||
		!fyto_icc_read_xyz(data, &gtag, colorant[1]) ||
		!fyto_icc_read_xyz(data, &btag, colorant[2]) ||
		!fyto_icc_build_curve(data, size, &rt, out->decode[0]) ||
		!fyto_icc_build_curve(data, size, &gt, out->decode[1]) ||
		!fyto_icc_build_curve(data, size, &bt, out->decode[2]))
	{
		out->state = FYTO_ICC_UNSUPPORTED;
		return;
	}

	/* matrix = D50->sRGB * [rXYZ gXYZ bXYZ] */
	for (i = 0; i < 3; i++)
		for (j = 0; j < 3; j++)
			out->matrix[i * 3 + j] =
				fyto_icc_d50_to_srgb[i * 3 + 0] * colorant[j][0] +
				fyto_icc_d50_to_srgb[i * 3 + 1] * colorant[j][1] +
				fyto_icc_d50_to_srgb[i * 3 + 2] * colorant[j][2];

	fyto_icc_build_encode(out);
	out->state = fyto_icc_is_identity(out) ? FYTO_ICC_IDENTITY : FYTO_ICC_MATRIX;
}

typedef struct fyto_icc_job_s
{
	BYTE *pixels;
	UINT wide, high, stride, bpp;
	UINT y0, y1;
	const fyto_icc_t *table;
} fyto_icc_job_t;

static void fyto_icc_rows(const fyto_icc_job_t *job)
{
	const fyto_icc_t *t = job->table;
	UINT x, y;
	for (y = job->y0; y < job->y1; y++)
	{
		BYTE *row = job->pixels + (SIZE_T)y * job->stride;
		if (job->bpp == 32)
		{
			for (x = 0; x < job->wide; x++)
			{
				BYTE *p = row + (SIZE_T)x * 4;
				int out[3];
				fyto_icc_map(t, p[2], p[1], p[0], out);
				p[0] = (BYTE)out[2];
				p[1] = (BYTE)out[1];
				p[2] = (BYTE)out[0];
			}
		}
		else
		{
			for (x = 0; x < job->wide; x++)
			{
				BYTE *p = row + (SIZE_T)x * 3;
				int out[3];
				fyto_icc_map(t, p[2], p[1], p[0], out);
				p[0] = (BYTE)out[2];
				p[1] = (BYTE)out[1];
				p[2] = (BYTE)out[0];
			}
		}
	}
}

static DWORD WINAPI fyto_icc_thread(void *param)
{
	fyto_icc_rows((const fyto_icc_job_t *)param);
	return 0;
}

/* Convert a BGR/BGRX bitmap in place. */
static void fyto_icc_apply(BYTE *pixels, UINT wide, UINT high, UINT stride, UINT bpp, const fyto_icc_t *t)
{
	fyto_icc_job_t jobs[8];
	HANDLE threads[8];
	SYSTEM_INFO info;
	UINT chunks, i, per;
	BOOL threaded;

	if (!pixels || !wide || !high || (bpp != 24 && bpp != 32)) return;
	if (t->state != FYTO_ICC_MATRIX) return;

	GetSystemInfo(&info);
	chunks = info.dwNumberOfProcessors ? info.dwNumberOfProcessors : 1;
	if (chunks > 8) chunks = 8;
	threaded = (UINT64)wide * high >= 512 * 1024 && chunks > 1;
	if (!threaded) chunks = 1;
	per = (high + chunks - 1) / chunks;

	for (i = 0; i < chunks; i++)
	{
		UINT y0 = i * per;
		UINT y1 = y0 + per > high ? high : y0 + per;
		jobs[i].pixels = pixels;
		jobs[i].wide = wide;
		jobs[i].high = high;
		jobs[i].stride = stride;
		jobs[i].bpp = bpp;
		jobs[i].y0 = y0;
		jobs[i].y1 = y1;
		jobs[i].table = t;
		threads[i] = NULL;
	}

	for (i = 0; i < chunks; i++)
	{
		if (jobs[i].y1 <= jobs[i].y0) continue;
		threads[i] = CreateThread(NULL, 0, fyto_icc_thread, &jobs[i], 0, NULL);
		if (!threads[i])
		{
			fyto_icc_rows(&jobs[i]); /* thread refused: do this slice inline */
			jobs[i].y1 = jobs[i].y0;
		}
	}

	for (i = 0; i < chunks; i++)
	{
		if (threads[i])
		{
			WaitForSingleObject(threads[i], INFINITE);
			CloseHandle(threads[i]);
		}
	}
}

#endif
