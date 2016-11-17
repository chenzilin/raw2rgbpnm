/*
 * raw2rgbpnm --- convert raw bayer images to RGB PNM for easier viewing
 *
 * Copyright (C) 2008--2011 Nokia Corporation
 *
 * Contact: Sakari Ailus <sakari.ailus@maxwell.research.nokia.com>
 *
 * Author:
 *	Tuukka Toivonen <tuukkat76@gmail.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <limits.h>
#include <linux/videodev2.h>
#include "utils.h"
#include "raw_to_rgb.h"
#include "yuv_to_rgb.h"

#ifndef V4L2_PIX_FMT_SGRBG10
#define V4L2_PIX_FMT_SGRBG10		v4l2_fourcc('B','A','1','0') /* 10bit raw bayer  */
#endif

#define DEFAULT_BGR 0

#define SIZE(x)		(sizeof(x)/sizeof((x)[0]))
#define MAX(a,b)	((a)>(b)?(a):(b))
#define MIN(a,b)	((a)<(b)?(a):(b))

char *progname = "yuv_to_rgbpnm";

static int swaprb = 0;
static int highbits = 0;			/* Bayer RAW10 formats use high bits for data */
static int brightness = 256;			/* 24.8 fixed point */

static const struct format_info {
	__u32 fmt;
	int bpp;		/* 0=variable, -1=unknown */
	char *name;
	unsigned int y_pos;
	unsigned int cb_pos;
} v4l2_pix_fmt_str[] = {
	{ V4L2_PIX_FMT_RGB332,   8,  "RGB332 (8  RGB-3-3-2)", 0, 0 },
	{ V4L2_PIX_FMT_RGB555,   16,  "RGB555 (16  RGB-5-5-5)", 0, 0 },
	{ V4L2_PIX_FMT_RGB565,   16,  "RGB565 (16  RGB-5-6-5)", 0, 0 },
	{ V4L2_PIX_FMT_RGB555X,  16,  "RGB555X (16  RGB-5-5-5 BE)", 0, 0 },
	{ V4L2_PIX_FMT_RGB565X,  16,  "RGB565X (16  RGB-5-6-5 BE)", 0, 0 },
	{ V4L2_PIX_FMT_BGR24,    24,  "BGR24 (24  BGR-8-8-8)", 0, 0 },
	{ V4L2_PIX_FMT_RGB24,    24,  "RGB24 (24  RGB-8-8-8)", 0, 0 },
	{ V4L2_PIX_FMT_BGR32,    32,  "BGR32 (32  BGR-8-8-8-8)", 0, 0 },
	{ V4L2_PIX_FMT_RGB32,    32,  "RGB32 (32  RGB-8-8-8-8)", 0, 0 },
	{ V4L2_PIX_FMT_GREY,     8,  "GREY (8  Greyscale)", 0, 0 },
	{ V4L2_PIX_FMT_Y10,      16,  "Y10 (10 Greyscale)", 0, 0 },
	{ V4L2_PIX_FMT_Y12,      16,  "Y12 (12 Greyscale)", 0, 0 },
	{ V4L2_PIX_FMT_UYVY,     16,  "UYVY (16  YUV 4:2:2)", 1, 0 },
	{ V4L2_PIX_FMT_VYUY,     16,  "VYUY (16  YUV 4:2:2)", 1, 2 },
	{ V4L2_PIX_FMT_YUYV,     16,  "YUYV (16  YUV 4:2:2)", 0, 1 },
	{ V4L2_PIX_FMT_YVYU,     16,  "YVYU (16  YUV 4:2:2)", 0, 3 },
	{ V4L2_PIX_FMT_YUV410,   -1,  "YUV410P (9  YUV 4:1:0 planar)", 0, 0 },
	{ V4L2_PIX_FMT_YVU410,   -1,  "YVU410P (9  YVU 4:1:0 planar)", 0, 1 },
	{ V4L2_PIX_FMT_YUV411P,  12,  "YUV411P (12  YUV 4:1:1 planar)", 0, 0 },
	{ V4L2_PIX_FMT_YUV420,   12,  "YUV420P (12  YUV 4:2:0 planar)", 0, 0 },
	{ V4L2_PIX_FMT_YVU420,   12,  "YVU420P (12  YVU 4:2:2 planar)", 0, 1 },
	{ V4L2_PIX_FMT_YUV422P,  16,  "YUV422P (16  YUV 4:2:2 planar)", 0, 0 },
	{ V4L2_PIX_FMT_YVU422M,  16,  "YVU422P (16  YVU 4:2:2 planar)", 0, 0 },
	{ V4L2_PIX_FMT_YUV444M,  24,  "YUV444P (24  YUV 4:4:4 planar)", 0, 0 },
	{ V4L2_PIX_FMT_YVU444M,  24,  "YVU444P (24  YVU 4:4:4 planar)", 0, 0 },
	{ V4L2_PIX_FMT_Y41P,     12,  "Y41P (12  YUV 4:1:1)", 0, 0 },
	{ V4L2_PIX_FMT_NV12,     12,  "NV12 (12  Y/CbCr 4:2:0)", 0, 0 },
	{ V4L2_PIX_FMT_NV21,     12,  "NV21 (12  Y/CrCb 4:2:0)", 0, 0 },
	{ V4L2_PIX_FMT_NV16,     16,  "NV16 (16  Y/CbCr 4:2:2)", 0, 0 },
	{ V4L2_PIX_FMT_NV61,     16,  "NV61 (16  Y/CrCb 4:2:2)", 0, 1 },
	{ V4L2_PIX_FMT_YYUV,     12,  "YYUV (16  YUV 4:2:2)", 0, 0 },
	{ V4L2_PIX_FMT_HI240,    8,  "HI240 (8  8-bit color)", 0, 0 },
//	{ V4L2_PIX_FMT_HM12,     8,  "HM12 (8  YUV 4:2:0 16x16 macroblocks)", 0, 0 },
	{ V4L2_PIX_FMT_SBGGR8,   8,  "SBGGR8 (8  BGBG.. GRGR..)", 0, 0 },
	{ V4L2_PIX_FMT_SGBRG8,   8,  "SGBRG8 (8  GBGB.. RGRG..)", 0, 0 },
	{ V4L2_PIX_FMT_SGRBG8,   8,  "SGRBG8 (8 GRGR.. BGBG..)", 0, 0 },
	{ V4L2_PIX_FMT_MJPEG,    0,  "MJPEG (Motion-JPEG)", 0, 0 },
	{ V4L2_PIX_FMT_JPEG,     0,  "JPEG (JFIF JPEG)", 0, 0 },
	{ V4L2_PIX_FMT_DV,       0,  "DV (1394)", 0, 0 },
	{ V4L2_PIX_FMT_MPEG,     0,  "MPEG (MPEG-1/2/4)", 0, 0 },
	{ V4L2_PIX_FMT_WNVA,     -1,  "WNVA (Winnov hw compress)", 0, 0 },
	{ V4L2_PIX_FMT_SN9C10X,  -1,  "SN9C10X (SN9C10x compression)", 0, 0 },
	{ V4L2_PIX_FMT_PWC1,     -1,  "PWC1 (pwc older webcam)", 0, 0 },
	{ V4L2_PIX_FMT_PWC2,     -1,  "PWC2 (pwc newer webcam)", 0, 0 },
	{ V4L2_PIX_FMT_ET61X251, -1,  "ET61X251 (ET61X251 compression)", 0, 0 },
	{ V4L2_PIX_FMT_SGRBG10,  16,  "SGRBG10 (10bit raw bayer)", 0, 0 },
	{ V4L2_PIX_FMT_SGRBG10DPCM8,    8, "SGRBG10DPCM8 (10bit raw bayer DPCM compressed to 8 bits)", 0, 0 },
	{ V4L2_PIX_FMT_SGRBG12,  16,  "SGRBG12 (12bit raw bayer)", 0, 0 },
	{ V4L2_PIX_FMT_SBGGR16,  16,  "SBGGR16 (16 BGBG.. GRGR..)", 0, 0 },
};

static void *xalloc(int size)
{
	void *b = calloc(1, size);
	if (!b) error("memory allocation failed");
	return b;
}

static const struct format_info *get_format_info(__u32 f)
{
	unsigned int i;

	for (i = 0; i < SIZE(v4l2_pix_fmt_str); i++) {
		if (v4l2_pix_fmt_str[i].fmt == f)
			return &v4l2_pix_fmt_str[i];
	};

	return NULL;
}

static const int resolutions[][2] = {
	{ 176, 144 },		/* QCIF */
	{ 320, 240 },		/* QVGA */
	{ 352, 288 },		/* CIF */
	{ 640, 480 },		/* VGA */
	{ 720, 576 },		/* PAL D1 */
	{ 768, 576 },		/* 1:1 aspect PAL D1 */
	{ 1920, 1440 },		/* 3VGA */
	{ 2560, 1920 },		/* 4VGA */
	{ 2592, 1944 },		/* 5 MP */
	{ 2592, 1968 },		/* 5 MP + a bit extra */
};

/* Read and return raw image data at given bits per pixel (bpp) depth.
 * size should be set correctly before calling this function.
 * If set to {-1,-1}, try to guess image file resolution.
 * If framenum is set to nonnegative value, assume that input file contains
 * multiple frames and return the given frame. In that case frame size must be given.
 */
static unsigned char *read_raw_data(char *filename, int framenum, int size[2], int bpp)
{
	/* Get file size */
	unsigned int line_length;
	unsigned int padding = 0;
	unsigned char *b = NULL;
	unsigned int i;
	int offset;
	FILE *f = fopen(filename, "rb");
	if (!f) error("fopen failed");
	int r = fseek(f, 0, SEEK_END);
	if (r!=0) error("fseek");
	int file_size = ftell(f);
	if (file_size==-1) error("ftell");
	r = fseek(f, 0, SEEK_SET);
	if (r!=0) error("fseek");

	/* Check image resolution */
	if (size[0]<=0 || size[1]<=0) {
		if (framenum>=0) error("can not automatically detect frame size with multiple frames");
		for (i=0; i<SIZE(resolutions); i++)
			if (resolutions[i][0]*resolutions[i][1]*bpp==file_size*8) break;
		if (i >= SIZE(resolutions)) error("can't guess raw image file resolution");
		size[0] = resolutions[i][0];
		size[1] = resolutions[i][1];
	}

	if (framenum<0 && (file_size*8 < size[0]*size[1]*bpp)) error("out of input data");
	if (framenum<0 && (file_size*8 > size[0]*size[1]*bpp)) printf("warning: too large image file\n");
	if (framenum < 0 && (file_size % size[1] == 0)) {
		line_length = size[0] * bpp / 8;
		padding = file_size / size[1] - line_length;
		printf("%u padding bytes detected at end of line\n", padding);
	} else if ((file_size * 8) % (size[0] * size[1] * bpp) != 0) {
		printf("warning: input size not multiple of frame size\n");
	}

	/* Go to the correct position in the file */
	if (framenum>=0) printf("Reading frame %i...\n", framenum);
	if (framenum<0) framenum = 0;
	offset = framenum*size[0]*size[1]*bpp/8;
	r = fseek(f, offset, SEEK_SET);
	if (r!=0) error("fseek");
	if ((file_size-offset)*8 < size[0]*size[1]*bpp) goto out;

	/* Read data */
	b = xalloc((size[0]*size[1]*bpp+7)/8);
	if (padding == 0) {
		r = fread(b, (size[0]*size[1]*bpp+7)/8, 1, f);
		if (r != 1)
			error("fread");
	} else {
		for (i = 0; i < (unsigned int)size[1]; ++i) {
			r = fread(b + i * line_length, line_length, 1, f);
			if (r != 1)
				error("fread");
			r = fseek(f, padding, SEEK_CUR);
			if (r != 0)
				error("fseek");
		}
	}
out:	fclose(f);
	return b;
}

static void raw_to_rgb(const struct format_info *info,
		       unsigned char *src, int src_size[2], unsigned char *rgb)
{
	unsigned int src_stride = src_size[0] * info->bpp / 8;
	unsigned int rgb_stride = src_size[0] * 3;
	unsigned char *src_luma, *src_chroma;
	unsigned char *src_cb, *src_cr;
	unsigned char *buf;
	unsigned int pixel;
	int r, g, b, a, cr, cb;
	int src_x, src_y;
	int dst_x, dst_y;
	int color_pos = 1;
	int y_pos;
	int cb_pos;
	int cr_pos;
	int shift = 0;

	switch (info->fmt) {
	case V4L2_PIX_FMT_VYUY:
	case V4L2_PIX_FMT_YVYU:
	case V4L2_PIX_FMT_UYVY:
	case V4L2_PIX_FMT_YUYV:		/* Packed YUV 4:2:2 */
		y_pos = info->y_pos;
		cb_pos = info->cb_pos;
		cr_pos = (info->cb_pos + 2) % 4;

		for (src_y = 0, dst_y = 0; dst_y < src_size[1]; src_y++, dst_y++) {
			for (src_x = 0, dst_x = 0; dst_x < src_size[0]; ) {
				cb = src[src_y*src_stride + src_x*4 + cb_pos];
				cr = src[src_y*src_stride + src_x*4 + cr_pos];

				a  = src[src_y*src_stride + src_x*4 + y_pos];
				yuv_to_rgb(a,cb,cr, &r, &g, &b);
				rgb[dst_y*rgb_stride+3*dst_x+0] = swaprb ? b : r;
				rgb[dst_y*rgb_stride+3*dst_x+1] = g;
				rgb[dst_y*rgb_stride+3*dst_x+2] = swaprb ? r : b;
				dst_x++;

				a  = src[src_y*src_stride + src_x*4 + y_pos + 2];
				yuv_to_rgb(a,cb,cr, &r, &g, &b);
				rgb[dst_y*rgb_stride+3*dst_x+0] = swaprb ? b : r;
				rgb[dst_y*rgb_stride+3*dst_x+1] = g;
				rgb[dst_y*rgb_stride+3*dst_x+2] = swaprb ? r : b;
				dst_x++;

				src_x++;
			}
		}
		break;

	case V4L2_PIX_FMT_NV21:
		color_pos = 0;
	case V4L2_PIX_FMT_NV12:
		src_luma = src;
		src_chroma = &src[src_size[0] * src_size[1]];
		src_stride = src_stride * 8 / 12;

		for (src_y = 0, dst_y = 0; dst_y < src_size[1]; src_y++, dst_y++) {
			cr = 0;

			for (dst_x = 0, src_x = 0; dst_x < src_size[0]; ) {
				a  = src_luma[dst_y*src_stride + dst_x];
				cb = src_chroma[(dst_y/2)*src_stride + dst_x + 1 - color_pos];
				yuv_to_rgb(a,cb,cr, &r, &g, &b);
				rgb[src_y*rgb_stride+3*src_x+0] = swaprb ? b : r;
				rgb[src_y*rgb_stride+3*src_x+1] = g;
				rgb[src_y*rgb_stride+3*src_x+2] = swaprb ? r : b;
				src_x++;
				dst_x++;

				a  = src_luma[dst_y*src_stride + dst_x];
				cr = src_chroma[(dst_y/2)*src_stride + dst_x + color_pos - 1];
				yuv_to_rgb(a,cb,cr, &r, &g, &b);
				rgb[src_y*rgb_stride+3*src_x+0] = swaprb ? b : r;
				rgb[src_y*rgb_stride+3*src_x+1] = g;
				rgb[src_y*rgb_stride+3*src_x+2] = swaprb ? r : b;
				src_x++;
				dst_x++;
			}
		}
		break;

	case V4L2_PIX_FMT_NV16:
	case V4L2_PIX_FMT_NV61:
		src_luma = src;
		src_chroma = &src[src_size[0] * src_size[1]];
		src_stride = src_stride * 8 / 16;

		cb_pos = info->cb_pos;
		cr_pos = 1 - info->cb_pos;

		for (src_y = 0, dst_y = 0; dst_y < src_size[1]; src_y++, dst_y++) {
			for (dst_x = 0, src_x = 0; dst_x < src_size[0]; ) {
				cb = src_chroma[dst_y*src_stride + dst_x + cb_pos];
				cr = src_chroma[dst_y*src_stride + dst_x + cr_pos];

				a  = src_luma[dst_y*src_stride + dst_x];
				yuv_to_rgb(a,cb,cr, &r, &g, &b);
				rgb[src_y*rgb_stride+3*src_x+0] = swaprb ? b : r;
				rgb[src_y*rgb_stride+3*src_x+1] = g;
				rgb[src_y*rgb_stride+3*src_x+2] = swaprb ? r : b;
				src_x++;
				dst_x++;

				a  = src_luma[dst_y*src_stride + dst_x];
				yuv_to_rgb(a,cb,cr, &r, &g, &b);
				rgb[src_y*rgb_stride+3*src_x+0] = swaprb ? b : r;
				rgb[src_y*rgb_stride+3*src_x+1] = g;
				rgb[src_y*rgb_stride+3*src_x+2] = swaprb ? r : b;
				src_x++;
				dst_x++;
			}
		}
		break;

	case V4L2_PIX_FMT_YUV411P:
		src_luma = src;
		src_cb = &src[src_size[0] * src_size[1]];
		src_cr = &src[src_size[0] * src_size[1] / 4 * 5];
		src_stride = src_stride * 8 / 12;

		for (src_y = 0, dst_y = 0; dst_y < src_size[1]; src_y++, dst_y++) {
			for (dst_x = 0, src_x = 0; dst_x < src_size[0]; ) {
				a  = src_luma[dst_y*src_stride + dst_x];
				cb = src_cb[dst_y*src_stride/4 + dst_x/4];
				cr = src_cr[dst_y*src_stride/4 + dst_x/4];

				yuv_to_rgb(a,cb,cr, &r, &g, &b);
				rgb[src_y*rgb_stride+3*src_x+0] = swaprb ? b : r;
				rgb[src_y*rgb_stride+3*src_x+1] = g;
				rgb[src_y*rgb_stride+3*src_x+2] = swaprb ? r : b;
				src_x++;
				dst_x++;
			}
		}
		break;

	case V4L2_PIX_FMT_YUV420:
	case V4L2_PIX_FMT_YVU420:
		src_luma = src;
		if (info->cb_pos == 0) {
			src_cb = &src[src_size[0] * src_size[1]];
			src_cr = &src[src_size[0] * src_size[1] / 4 * 5];
		} else {
			src_cr = &src[src_size[0] * src_size[1]];
			src_cb = &src[src_size[0] * src_size[1] / 4 * 5];
		}
		src_stride = src_stride * 8 / 12;

		for (src_y = 0, dst_y = 0; dst_y < src_size[1]; src_y++, dst_y++) {
			for (dst_x = 0, src_x = 0; dst_x < src_size[0]; ) {
				a  = src_luma[dst_y*src_stride + dst_x];
				cb = src_cb[(dst_y/2)*src_stride/2 + dst_x/2];
				cr = src_cr[(dst_y/2)*src_stride/2 + dst_x/2];

				yuv_to_rgb(a,cb,cr, &r, &g, &b);
				rgb[src_y*rgb_stride+3*src_x+0] = swaprb ? b : r;
				rgb[src_y*rgb_stride+3*src_x+1] = g;
				rgb[src_y*rgb_stride+3*src_x+2] = swaprb ? r : b;
				src_x++;
				dst_x++;
			}
		}
		break;

	case V4L2_PIX_FMT_YUV422P:
	case V4L2_PIX_FMT_YVU422M:
		src_luma = src;
		if (info->cb_pos == 0) {
			src_cb = &src[src_size[0] * src_size[1]];
			src_cr = &src[src_size[0] * src_size[1] / 2 * 3];
		} else {
			src_cr = &src[src_size[0] * src_size[1]];
			src_cb = &src[src_size[0] * src_size[1] / 2 * 3];
		}
		src_stride = src_stride * 8 / 16;

		for (src_y = 0, dst_y = 0; dst_y < src_size[1]; src_y++, dst_y++) {
			for (dst_x = 0, src_x = 0; dst_x < src_size[0]; ) {
				a  = src_luma[dst_y*src_stride + dst_x];
				cb = src_cb[dst_y*src_stride/2 + dst_x/2];
				cr = src_cr[dst_y*src_stride/2 + dst_x/2];

				yuv_to_rgb(a,cb,cr, &r, &g, &b);
				rgb[src_y*rgb_stride+3*src_x+0] = swaprb ? b : r;
				rgb[src_y*rgb_stride+3*src_x+1] = g;
				rgb[src_y*rgb_stride+3*src_x+2] = swaprb ? r : b;
				src_x++;
				dst_x++;
			}
		}
		break;

	case V4L2_PIX_FMT_YUV444M:
	case V4L2_PIX_FMT_YVU444M:
		src_luma = src;
		if (info->cb_pos == 0) {
			src_cb = &src[src_size[0] * src_size[1]];
			src_cr = &src[src_size[0] * src_size[1] * 2];
		} else {
			src_cr = &src[src_size[0] * src_size[1]];
			src_cb = &src[src_size[0] * src_size[1] * 2];
		}
		src_stride = src_stride * 8 / 24;

		for (src_y = 0, dst_y = 0; dst_y < src_size[1]; src_y++, dst_y++) {
			for (dst_x = 0, src_x = 0; dst_x < src_size[0]; ) {
				a  = src_luma[dst_y*src_stride + dst_x];
				cb = src_cb[dst_y*src_stride + dst_x];
				cr = src_cr[dst_y*src_stride + dst_x];

				yuv_to_rgb(a,cb,cr, &r, &g, &b);
				rgb[src_y*rgb_stride+3*src_x+0] = swaprb ? b : r;
				rgb[src_y*rgb_stride+3*src_x+1] = g;
				rgb[src_y*rgb_stride+3*src_x+2] = swaprb ? r : b;
				src_x++;
				dst_x++;
			}
		}
		break;

	case V4L2_PIX_FMT_Y12:
		shift += 2;
	case V4L2_PIX_FMT_Y10:
		shift += 2;
		for (src_y = 0, dst_y = 0; dst_y < src_size[1]; src_y++, dst_y++) {
			for (src_x = 0, dst_x = 0; dst_x < src_size[0]; ) {
				a = (src[src_y*src_stride + src_x*2+0] |
				     (src[src_y*src_stride + src_x*2+1] << 8)) >> shift;
				rgb[dst_y*rgb_stride+3*dst_x+0] = a;
				rgb[dst_y*rgb_stride+3*dst_x+1] = a;
				rgb[dst_y*rgb_stride+3*dst_x+2] = a;
				src_x++;
				dst_x++;
			}
		}
		break;

	case V4L2_PIX_FMT_GREY:
		for (src_y = 0, dst_y = 0; dst_y < src_size[1]; src_y++, dst_y++) {
			for (src_x = 0, dst_x = 0; dst_x < src_size[0]; ) {
				a = src[src_y*src_stride + src_x];
				rgb[dst_y*rgb_stride+3*dst_x+0] = a;
				rgb[dst_y*rgb_stride+3*dst_x+1] = a;
				rgb[dst_y*rgb_stride+3*dst_x+2] = a;
				src_x++;
				dst_x++;
			}
		}
		break;

	case V4L2_PIX_FMT_SBGGR16:
		printf("WARNING: bayer phase not supported -> expect bad colors\n");
		shift += 4;
	case V4L2_PIX_FMT_SGRBG12:
		shift += 2;
	case V4L2_PIX_FMT_SGRBG10:
		for (dst_y=0; dst_y<src_size[1]; dst_y++) {
			for (dst_x=0; dst_x<src_size[0]; dst_x++) {
				unsigned short *p = (unsigned short *)&(src[src_stride*dst_y+dst_x*2]);
				int v = *p;
				if (highbits)
					v >>= 6;
				else
					v >>= shift;
				if (v<0 || v>=(1<<10))
					printf("WARNING: bayer image pixel values out of range (%i)\n", v);
				v *= brightness;
				v >>= 8;
				if (v < 0) v = 0;
				if (v >= (1<<10)) v = (1<<10)-1;
				*p = v;
			}
		}

		buf = malloc(src_size[0] * src_size[1] * 3);
		if (buf==NULL) error("out of memory");
		qc_imag_bay2rgb10(src, src_stride, buf, src_size[0]*3, src_size[0], src_size[1], 3);
		for (dst_y=0; dst_y<src_size[1]; dst_y++) {
			for (dst_x=0; dst_x<src_size[0]; dst_x++) {
				unsigned char *p = buf + src_size[0]*3*dst_y + dst_x*3;
				rgb[dst_y*rgb_stride+3*dst_x+0] = swaprb ? p[2] : p[0];
				rgb[dst_y*rgb_stride+3*dst_x+1] = p[1];
				rgb[dst_y*rgb_stride+3*dst_x+2] = swaprb ? p[0] : p[2];
			}
		}
		free(buf);
		break;
	case V4L2_PIX_FMT_SBGGR8:
	case V4L2_PIX_FMT_SGBRG8:
		printf("WARNING: bayer phase not supported -> expect bad colors\n");
	case V4L2_PIX_FMT_SGRBG8:
		/* FIXME: only SGRBG8 handled properly: color phase is ignored. */
		buf = malloc(src_size[0] * src_size[1] * 3);
		if (buf==NULL) error("out of memory");
		qc_imag_bay2rgb8(src, src_stride, buf, src_size[0]*3, src_size[0], src_size[1], 3);
		for (dst_y=0; dst_y<src_size[1]; dst_y++) {
			for (dst_x=0; dst_x<src_size[0]; dst_x++) {
				unsigned char *p = buf + src_size[0]*3*dst_y + dst_x*3;
				rgb[dst_y*rgb_stride+3*dst_x+0] = swaprb ? p[2] : p[0];
				rgb[dst_y*rgb_stride+3*dst_x+1] = p[1];
				rgb[dst_y*rgb_stride+3*dst_x+2] = swaprb ? p[0] : p[2];
			}
		}
		free(buf);
		break;
	case V4L2_PIX_FMT_RGB332:
		for (src_y = 0, dst_y = 0; dst_y < src_size[1]; src_y++, dst_y++) {
			for (src_x = 0, dst_x = 0; dst_x < src_size[0]; ) {
				pixel = src[dst_y*src_stride + dst_x];
				r = (pixel << 0) & 0xe0;
				g = (pixel << 3) & 0xe0;
				b = (pixel << 6) & 0xc0;
				rgb[src_y*rgb_stride+3*src_x+0] = swaprb ? b : r;
				rgb[src_y*rgb_stride+3*src_x+1] = g;
				rgb[src_y*rgb_stride+3*src_x+2] = swaprb ? r : b;
				src_x++;
				dst_x++;
			}
		}
		break;
	case V4L2_PIX_FMT_RGB555:
		for (src_y = 0, dst_y = 0; dst_y < src_size[1]; src_y++, dst_y++) {
			for (src_x = 0, dst_x = 0; dst_x < src_size[0]; ) {
				pixel = (src[dst_y*src_stride + dst_x*2 + 1] << 8)
				      | src[dst_y*src_stride + dst_x*2 + 0];
				r = (pixel >> 7) & 0xf8;
				g = (pixel >> 2) & 0xf8;
				b = (pixel << 3) & 0xf8;
				rgb[src_y*rgb_stride+3*src_x+0] = swaprb ? b : r;
				rgb[src_y*rgb_stride+3*src_x+1] = g;
				rgb[src_y*rgb_stride+3*src_x+2] = swaprb ? r : b;
				src_x++;
				dst_x++;
			}
		}
		break;
	case V4L2_PIX_FMT_RGB565:
		for (src_y = 0, dst_y = 0; dst_y < src_size[1]; src_y++, dst_y++) {
			for (src_x = 0, dst_x = 0; dst_x < src_size[0]; ) {
				pixel = (src[dst_y*src_stride + dst_x*2 + 1] << 8)
				      | src[dst_y*src_stride + dst_x*2 + 0];
				r = (pixel >> 8) & 0xf8;
				g = (pixel >> 3) & 0xfc;
				b = (pixel << 3) & 0xf8;
				rgb[src_y*rgb_stride+3*src_x+0] = swaprb ? b : r;
				rgb[src_y*rgb_stride+3*src_x+1] = g;
				rgb[src_y*rgb_stride+3*src_x+2] = swaprb ? r : b;
				src_x++;
				dst_x++;
			}
		}
		break;
	case V4L2_PIX_FMT_BGR24:
		swaprb = !swaprb;
		/* Fallthrough */
	case V4L2_PIX_FMT_RGB24:
		for (src_y = 0, dst_y = 0; dst_y < src_size[1]; src_y++, dst_y++) {
			for (src_x = 0, dst_x = 0; dst_x < src_size[0]; ) {
				r = src[dst_y*src_stride + dst_x*3 + 0];
				g = src[dst_y*src_stride + dst_x*3 + 1];
				b = src[dst_y*src_stride + dst_x*3 + 2];
				rgb[src_y*rgb_stride+3*src_x+0] = swaprb ? b : r;
				rgb[src_y*rgb_stride+3*src_x+1] = g;
				rgb[src_y*rgb_stride+3*src_x+2] = swaprb ? r : b;
				src_x++;
				dst_x++;
			}
		}
		break;
	case V4L2_PIX_FMT_BGR32:
		for (src_y = 0, dst_y = 0; dst_y < src_size[1]; src_y++, dst_y++) {
			for (src_x = 0, dst_x = 0; dst_x < src_size[0]; ) {
				r = src[dst_y*src_stride + dst_x*4 + 2];
				g = src[dst_y*src_stride + dst_x*4 + 1];
				b = src[dst_y*src_stride + dst_x*4 + 0];
				rgb[src_y*rgb_stride+3*src_x+0] = swaprb ? b : r;
				rgb[src_y*rgb_stride+3*src_x+1] = g;
				rgb[src_y*rgb_stride+3*src_x+2] = swaprb ? r : b;
				src_x++;
				dst_x++;
			}
		}
		break;
	case V4L2_PIX_FMT_RGB32:
		for (src_y = 0, dst_y = 0; dst_y < src_size[1]; src_y++, dst_y++) {
			for (src_x = 0, dst_x = 0; dst_x < src_size[0]; ) {
				r = src[dst_y*src_stride + dst_x*4 + 1];
				g = src[dst_y*src_stride + dst_x*4 + 2];
				b = src[dst_y*src_stride + dst_x*4 + 3];
				rgb[src_y*rgb_stride+3*src_x+0] = swaprb ? b : r;
				rgb[src_y*rgb_stride+3*src_x+1] = g;
				rgb[src_y*rgb_stride+3*src_x+2] = swaprb ? r : b;
				src_x++;
				dst_x++;
			}
		}
		break;
	}
}

static int parse_format(const char *p, int *w, int *h)
{
	char *end;

	for (; isspace(*p); ++p);

	*w = strtoul(p, &end, 10);
	if (*end != 'x')
		return -1;

	p = end + 1;
	*h = strtoul(p, &end, 10);
	if (*end != '\0')
		return -1;

	return 0;
}

int main(int argc, char *argv[])
{
	FILE *f;
	int size[2] = {-1,-1};
	unsigned char *src, *dst;
	char *file_in = NULL, *file_out = NULL;
	char multi_file_out[NAME_MAX];
	int format = V4L2_PIX_FMT_UYVY;
	const struct format_info *info;
	int r;
	char *algorithm_name = NULL;
	int n = 0, multiple = 0;

	for (;;) {
		int c = getopt(argc, argv, "a:b:f:ghs:w");
		if (c==-1) break;
		switch (c) {
		case 'a':
			if (optarg[0]=='?') {
				printf("Available bayer-to-rgb conversion algorithms:\n");
				qc_print_algorithms();
				exit(0);
			}
			algorithm_name = optarg;
			break;
		case 'b':
			brightness = (int)(atof(optarg) * 256.0 + 0.5);
			break;
		case 'f':
			if (optarg[0]=='?' && optarg[1]==0) {
				unsigned int i,j;
				printf("Supported formats:\n");
				for (i=0; i<SIZE(v4l2_pix_fmt_str); i++) {
					for (j=0; v4l2_pix_fmt_str[i].name[j]!=' ' && v4l2_pix_fmt_str[i].name[j]!=0; j++)
						putchar(v4l2_pix_fmt_str[i].name[j]);
					printf("\n");
				};
				exit(0);
			} else {
				unsigned int i,j;
				for (i=0; i<SIZE(v4l2_pix_fmt_str); i++) {
					for (j=0; v4l2_pix_fmt_str[i].name[j]!=' ' && v4l2_pix_fmt_str[i].name[j]!=0; j++);
					if (memcmp(v4l2_pix_fmt_str[i].name, optarg, j)==0 &&
					    optarg[j]==0) break;
				};
				if (i >= SIZE(v4l2_pix_fmt_str)) error("bad format");
				format = v4l2_pix_fmt_str[i].fmt;
			}
			break;
		case 'g':
			highbits = 1;
			break;
		case 'h':
			printf("%s - Convert headerless raw image to RGB file (PNM)\n"
			       "Usage: %s [-h] [-w] [-s XxY] <inputfile> <outputfile>\n"
			       "-a <algo>     Select algorithm, use \"-a ?\" for a list\n"
			       "-b <bright>   Set brightness (multiplier) to output image (float, default 1.0)\n"
			       "-f <format>   Specify input file format format (-f ? for list, default UYVY)\n"
			       "-g            Use high bits for Bayer RAW 10 data\n"
			       "-h            Show this help\n",
			       "-n            Assume multiple input frames, extract several PNM files\n"
			       "-s <XxY>      Specify image size\n"
			       "-w            Swap R and B channels\n", argv[0]);
			exit(0);
		case 'n':
			multiple = 1;
			break;
		case 's':
			if (parse_format(optarg, &size[0], &size[1]) < 0) {
				error("bad size");
				exit(0);
			}
			break;
		case 'w':
			swaprb = 1;
			break;
		default:
			error("bad argument");
		}
	}

	if (algorithm_name != NULL) qc_set_algorithm(algorithm_name);
	if (argc-optind != 2) error("give input and output files");
	file_in  = argv[optind++];
	file_out = argv[optind++];

	info = get_format_info(format);
	if (info == NULL) {
		printf("unsupported video format %4.4s\n", (char *)&format);
		return 1;
	}

	/* Read, convert, and save image */
	src = read_raw_data(file_in, multiple ? 0 : -1, size, info->bpp);
	printf("Image size: %ix%i, bytes per pixel: %i, format: %s\n", size[0], size[1],
		info->bpp, info->name);
	dst = xalloc(size[0]*size[1]*3);
	do {
		raw_to_rgb(info, src, size, dst);
		sprintf(multi_file_out, "%s-%03i.pnm", file_out, n);
		printf("Writing to file `%s'...\n", multiple ? multi_file_out : file_out);
		f = fopen(multiple ? multi_file_out : file_out, "wb");
		if (!f) error("file open failed");
		fprintf(f, "P6\n%i %i\n255\n", size[0], size[1]);
		r = fwrite(dst, size[0]*size[1]*3, 1, f);
		if (r!=1) error("write failed");
		fclose(f);
		if (!multiple) break;
		src = read_raw_data(file_in, ++n, size, info->bpp);
	} while (src != NULL);
	free(src);
	free(dst);
	return 0;
}
