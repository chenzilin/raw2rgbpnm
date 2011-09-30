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

#define V4L2_PIX_FMT_BAYER10_GrRBGb	1234567
#ifndef V4L2_PIX_FMT_SGRBG10
#define V4L2_PIX_FMT_SGRBG10		v4l2_fourcc('B','A','1','0') /* 10bit raw bayer  */
#endif

#define DEFAULT_BGR 0

#define SIZE(x)		(sizeof(x)/sizeof((x)[0]))
#define MAX(a,b)	((a)>(b)?(a):(b))
#define MIN(a,b)	((a)<(b)?(a):(b))

char *progname = "yuv_to_rgbpnm";

static const int downscaling = 1;
static int swaprb = 0;
static int highbits = 0;			/* Bayer RAW10 formats use high bits for data */
static int brightness = 256;			/* 24.8 fixed point */

/* {{{ [fold] v4l2_pix_fmt_str */
static const struct {
	__u32 fmt;
	int bpp;		/* 0=variable, -1=unknown */
	char *name;
} v4l2_pix_fmt_str[] = {
	{ V4L2_PIX_FMT_RGB332,   8,  "RGB332 (8  RGB-3-3-2)" },
	{ V4L2_PIX_FMT_RGB555,   16,  "RGB555 (16  RGB-5-5-5)" },
	{ V4L2_PIX_FMT_RGB565,   16,  "RGB565 (16  RGB-5-6-5)" },
	{ V4L2_PIX_FMT_RGB555X,  16,  "RGB555X (16  RGB-5-5-5 BE)" },
	{ V4L2_PIX_FMT_RGB565X,  16,  "RGB565X (16  RGB-5-6-5 BE)" },
	{ V4L2_PIX_FMT_BGR24,    24,  "BGR24 (24  BGR-8-8-8)" },
	{ V4L2_PIX_FMT_RGB24,    24,  "RGB24 (24  RGB-8-8-8)" },
	{ V4L2_PIX_FMT_BGR32,    32,  "BGR32 (32  BGR-8-8-8-8)" },
	{ V4L2_PIX_FMT_RGB32,    32,  "RGB32 (32  RGB-8-8-8-8)" },
	{ V4L2_PIX_FMT_GREY,     8,  "GREY (8  Greyscale)" },
	{ V4L2_PIX_FMT_YVU410,   -1,  "YVU410 (9  YVU 4:1:0)" },
	{ V4L2_PIX_FMT_YVU420,   12,  "YVU420 (12  YVU 4:2:0)" },
	{ V4L2_PIX_FMT_YUYV,     16,  "YUYV (16  YUV 4:2:2)" },
	{ V4L2_PIX_FMT_UYVY,     16,  "UYVY (16  YUV 4:2:2)" },
	{ V4L2_PIX_FMT_YUV422P,  16,  "YUV422P (16  YVU422 planar)" },
	{ V4L2_PIX_FMT_YUV411P,  16,  "YUV411P (16  YVU411 planar)" },
	{ V4L2_PIX_FMT_Y41P,     12,  "Y41P (12  YUV 4:1:1)" },
	{ V4L2_PIX_FMT_NV12,     12,  "NV12 (12  Y/CbCr 4:2:0)" },
	{ V4L2_PIX_FMT_NV21,     12,  "NV21 (12  Y/CrCb 4:2:0)" },
	{ V4L2_PIX_FMT_YUV410,   -1,  "YUV410 (9  YUV 4:1:0)" },
	{ V4L2_PIX_FMT_YUV420,   12,  "YUV420 (12  YUV 4:2:0)" },
	{ V4L2_PIX_FMT_YYUV,     12,  "YYUV (16  YUV 4:2:2)" },
	{ V4L2_PIX_FMT_HI240,    8,  "HI240 (8  8-bit color)" },
//	{ V4L2_PIX_FMT_HM12,     8,  "HM12 (8  YUV 4:2:0 16x16 macroblocks)" },
	{ V4L2_PIX_FMT_SBGGR8,   8,  "SBGGR8 (8  BGBG.. GRGR..)" },
	{ V4L2_PIX_FMT_SGBRG8,   8,  "SGBRG8 (8  GBGB.. RGRG..)" },
	{ V4L2_PIX_FMT_SGRBG8,   8,  "SGRBG8 (8 GRGR.. BGBG..)" },
	{ V4L2_PIX_FMT_MJPEG,    0,  "MJPEG (Motion-JPEG)" },
	{ V4L2_PIX_FMT_JPEG,     0,  "JPEG (JFIF JPEG)" },
	{ V4L2_PIX_FMT_DV,       0,  "DV (1394)" },
	{ V4L2_PIX_FMT_MPEG,     0,  "MPEG (MPEG-1/2/4)" },
	{ V4L2_PIX_FMT_WNVA,     -1,  "WNVA (Winnov hw compress)" },
	{ V4L2_PIX_FMT_SN9C10X,  -1,  "SN9C10X (SN9C10x compression)" },
	{ V4L2_PIX_FMT_PWC1,     -1,  "PWC1 (pwc older webcam)" },
	{ V4L2_PIX_FMT_PWC2,     -1,  "PWC2 (pwc newer webcam)" },
	{ V4L2_PIX_FMT_ET61X251, -1,  "ET61X251 (ET61X251 compression)" },
	{ V4L2_PIX_FMT_BAYER10_GrRBGb, -1, "BAYER10_GrRBGb (from sensor_if.h)" },
	{ V4L2_PIX_FMT_SGRBG10,  16,  "SGRBG10 (10bit raw bayer)" },
	{ V4L2_PIX_FMT_SGRBG10DPCM8,    8, "SGRBG10DPCM8 (10bit raw bayer DPCM compressed to 8 bits)" },
	{ V4L2_PIX_FMT_SBGGR16,  16,  "SBGGR16 (16 BGBG.. GRGR..)" },
};
/* }}} */

/* {{{ [fold] static void *xalloc(int size) */
static void *xalloc(int size)
{
	void *b = calloc(1, size);
	if (!b) error("memory allocation failed");
	return b;
}
/* }}} */
/* {{{ [fold] static const char *get_pix_fmt(__u32 f) */
static const char *get_pix_fmt(__u32 f)
{
	int i;
	for (i=0; i<SIZE(v4l2_pix_fmt_str); i++) {
		if (v4l2_pix_fmt_str[i].fmt==f) return v4l2_pix_fmt_str[i].name;
	};
	return "(unknown)";
}
/* }}} */
/* {{{ [fold] static int get_pix_bpp(__u32 f) */
static int get_pix_bpp(__u32 f)
{
	int i;
	for (i=0; i<SIZE(v4l2_pix_fmt_str); i++) {
		if (v4l2_pix_fmt_str[i].fmt==f) return v4l2_pix_fmt_str[i].bpp;
	};
	return -1;
}
/* }}} */
/* {{{ [fold] static unsigned char *read_raw_data(char *filename, int framenum, int size[2], int bpp) */
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
	unsigned char *b = NULL;
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
		unsigned int i;
		if (framenum>=0) error("can not automatically detect frame size with multiple frames");
		for (i=0; i<SIZE(resolutions); i++)
			if (resolutions[i][0]*resolutions[i][1]*bpp==file_size*8) break;
		if (i >= SIZE(resolutions)) error("can't guess raw image file resolution");
		size[0] = resolutions[i][0];
		size[1] = resolutions[i][1];
	}

	if (framenum<0 && (file_size*8 < size[0]*size[1]*bpp)) error("out of input data");
	if (framenum<0 && (file_size*8 > size[0]*size[1]*bpp)) printf("warning: too large image file\n");
	if (((file_size*8) % (size[0]*size[1]*bpp)) != 0) printf("warning: input size not multiple of frame size\n");

	/* Go to the correct position in the file */
	if (framenum>=0) printf("Reading frame %i...\n", framenum);
	if (framenum<0) framenum = 0;
	offset = framenum*size[0]*size[1]*bpp/8;
	r = fseek(f, offset, SEEK_SET);
	if (r!=0) error("fseek");
	if ((file_size-offset)*8 < size[0]*size[1]*bpp) goto out;

	/* Read data */
	b = xalloc((size[0]*size[1]*bpp+7)/8);
	r = fread(b, (size[0]*size[1]*bpp+7)/8, 1, f);
	if (r!=1) error("fread");
out:	fclose(f);
	return b;
}
/* }}} */
/* {{{ [fold] static void raw_to_rgb(unsigned char *src, int src_stride, int size[2], int format, unsigned char *rgb, int rgb_stride) */
static void raw_to_rgb(unsigned char *src, int src_stride, int src_size[2], int format, unsigned char *rgb, int rgb_stride)
{
	unsigned char *buf;
	int r, g, b, a, cr, cb;
	int scr_x, scr_y;
	int scr_step;
	int img_x, img_y;
	int img_step;
	int color_pos = 1;

	scr_step = 1;
	img_step = downscaling;
	scr_y = 0;
	img_y = 0;

	switch (format) {
	default:
		printf("copy_to_framebuffer: unsupported video format %s (trying YUYV)\n", get_pix_fmt(format));
	case V4L2_PIX_FMT_UYVY:
		color_pos = -1;
		src++;
		/* Continue */
	case V4L2_PIX_FMT_YUYV:		/* Packed YUV 4:2:2; FIXME: downscale should be odd, otherwise cr is wrong */
		do {
			scr_x = 0;
			img_x = 0;
			cr = 0;
			do {
				a  = src[img_y*src_stride + img_x*2];
				cb = src[img_y*src_stride + img_x*2 + color_pos];
				yuv_to_rgb(a,cb,cr, &r, &g, &b);
				rgb[scr_y*rgb_stride+3*scr_x+0] = swaprb ? b : r;
				rgb[scr_y*rgb_stride+3*scr_x+1] = g;
				rgb[scr_y*rgb_stride+3*scr_x+2] = swaprb ? r : b;
				scr_x += scr_step;
				img_x += img_step;

				a  = src[img_y*src_stride + img_x*2];
				cr = src[img_y*src_stride + img_x*2 + color_pos];
				yuv_to_rgb(a,cb,cr, &r, &g, &b);
				rgb[scr_y*rgb_stride+3*scr_x+0] = swaprb ? b : r;
				rgb[scr_y*rgb_stride+3*scr_x+1] = g;
				rgb[scr_y*rgb_stride+3*scr_x+2] = swaprb ? r : b;
				scr_x += scr_step;
				img_x += img_step;
			} while (img_x<src_size[0]);
			scr_y += scr_step;
			img_y += img_step;
		} while (img_y<src_size[1]);
		break;
	case V4L2_PIX_FMT_SBGGR16:
		printf("WARNING: bayer phase not supported -> expect bad colors\n");
	case V4L2_PIX_FMT_BAYER10_GrRBGb:
	case V4L2_PIX_FMT_SGRBG10:
		for (img_y=0; img_y<src_size[1]; img_y++) {
			for (img_x=0; img_x<src_size[0]; img_x++) {
				unsigned short *p = (unsigned short *)&(src[src_stride*img_y+img_x*2]);
				int v = *p;
				if (highbits) v >>= 6;
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
		for (img_y=0; img_y<src_size[1]; img_y++) {
			for (img_x=0; img_x<src_size[0]; img_x++) {
				unsigned char *p = buf + src_size[0]*3*img_y + img_x*3;
				rgb[img_y*rgb_stride+3*img_x+0] = swaprb ? p[2] : p[0];
				rgb[img_y*rgb_stride+3*img_x+1] = p[1];
				rgb[img_y*rgb_stride+3*img_x+2] = swaprb ? p[0] : p[2];
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
		for (img_y=0; img_y<src_size[1]; img_y++) {
			for (img_x=0; img_x<src_size[0]; img_x++) {
				unsigned char *p = buf + src_size[0]*3*img_y + img_x*3;
				rgb[img_y*rgb_stride+3*img_x+0] = swaprb ? p[2] : p[0];
				rgb[img_y*rgb_stride+3*img_x+1] = p[1];
				rgb[img_y*rgb_stride+3*img_x+2] = swaprb ? p[0] : p[2];
			}
		}
		free(buf);
		break;
	}
}
/* }}} */

static int parse_format(const char *p, unsigned int *w, unsigned int *h)
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

/* {{{ [fold] int main(int argc, char *argv[]) */
int main(int argc, char *argv[])
{
	FILE *f;
	int size[2] = {-1,-1};
	int size2[2];			/* Size after cropping */
	int crop[2] = {0,0};
	unsigned char *src, *dst;
	unsigned char *src2;		/* Origin of cropped image */
	char *file_in = NULL, *file_out = NULL;
	char multi_file_out[NAME_MAX];
	int format = V4L2_PIX_FMT_UYVY;
	int r;
	char *algorithm_name = NULL;
	int n = 0, multiple = 0;

	for (;;) {
		int c = getopt(argc, argv, "gwr:b:a:s:c:nh");
		if (c==-1) break;
		switch (c) {
		case 'w':
			swaprb = 1;
			break;
		case 'g':
			highbits = 1;
			break;
		case 'r':
			if (optarg[0]=='?' && optarg[1]==0) {
				int i,j;
				printf("Supported formats:\n");
				for (i=0; i<SIZE(v4l2_pix_fmt_str); i++) {
					for (j=0; v4l2_pix_fmt_str[i].name[j]!=' ' && v4l2_pix_fmt_str[i].name[j]!=0; j++)
						putchar(v4l2_pix_fmt_str[i].name[j]);
					printf("\n");
				};
				exit(0);
			} else {
				int i,j;
				for (i=0; i<SIZE(v4l2_pix_fmt_str); i++) {
					for (j=0; v4l2_pix_fmt_str[i].name[j]!=' ' && v4l2_pix_fmt_str[i].name[j]!=0; j++);
					if (memcmp(v4l2_pix_fmt_str[i].name, optarg, j)==0 &&
					    optarg[j]==0) break;
				};
				if (i >= SIZE(v4l2_pix_fmt_str)) error("bad format");
				format = v4l2_pix_fmt_str[i].fmt;
			}
			break;
		case 'b':
			brightness = (int)(atof(optarg) * 256.0 + 0.5);
			break;
		case 'a':
			if (optarg[0]=='?') {
				printf("Available bayer-to-rgb conversion algorithms:\n");
				qc_print_algorithms();
				exit(0);
			}
			algorithm_name = optarg;
			break;
		case 's':
			if (parse_format(optarg, &size[0], &size[1]) < 0) {
				error("bad size");
				exit(0);
			}
			break;
		case 'c':
			crop[0] = atoi(optarg);
			crop[1] = atoi(strchr(optarg,',')+1);
			printf("Configured cropping of %i,%i\n", crop[0], crop[1]);
			break;
		case 'n':
			multiple = 1;
			break;
		case 'h':
			printf("%s - Convert headerless raw image to RGB file (PNM)\n"
			       "Usage: %s [-h] [-w] [-s XxY] <inputfile> <outputfile>\n"
			       "-s <XxY>      Specify image size\n"
			       "-c <X,Y>      Crop from left and top this many pixels\n"
			       "-r <format>   Specify input file format format (-r ? for list, default UYVY)\n"
			       "-b <bright>   Set brightness (multiplier) to output image (float, default 1.0)\n"
			       "-a <algo>     Select algorithm, use \"-a ?\" for a list\n"
			       "-w            Swap R and B channels\n"
			       "-g            Use high bits for Bayer RAW 10 data\n"
			       "-n            Assume multiple input frames, extract several PNM files\n"
			       "-h            Show this help\n", progname, argv[0]);
			exit(0);
		default:
			error("bad argument");
		}
	}

	if (algorithm_name != NULL) qc_set_algorithm(algorithm_name);
	if (argc-optind != 2) error("give input and output files");
	file_in  = argv[optind++];
	file_out = argv[optind++];

	/* Read, convert, and save image */
	src = read_raw_data(file_in, multiple ? 0 : -1, size, get_pix_bpp(format));
	printf("Image size: %ix%i, bytes per pixel: %i, format: %s\n", size[0], size[1],
		get_pix_bpp(format), get_pix_fmt(format));
	size2[0] = size[0] - crop[0];
	size2[1] = size[1] - crop[1];
	dst = xalloc(size2[0]*size2[1]*3);
	do {
		src2 = src + (crop[0] + crop[1]*size[0])*get_pix_bpp(format)/8;
		raw_to_rgb(src2, size[0]*get_pix_bpp(format)/8, size2, format, dst, size2[0]*3);
		sprintf(multi_file_out, "%s-%03i.pnm", file_out, n);
		printf("Writing to file `%s'...\n", multiple ? multi_file_out : file_out);
		f = fopen(multiple ? multi_file_out : file_out, "wb");
		if (!f) error("file open failed");
		fprintf(f, "P6\n%i %i\n255\n", size2[0], size2[1]);
		r = fwrite(dst, size2[0]*size2[1]*3, 1, f);
		if (r!=1) error("write failed");
		fclose(f);
		if (!multiple) break;
		src = read_raw_data(file_in, ++n, size, get_pix_bpp(format));
	} while (src != NULL);
	free(src);
	free(dst);
	return 0;
}
/* }}} */

/* EOF */
