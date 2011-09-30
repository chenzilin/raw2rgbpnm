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
#include <string.h>
#include "raw_to_rgb.h"

#define DEFAULT_BGR 0
#define DETECT_BADVAL 1

#define MAX(a,b)	((a)>(b)?(a):(b))
#define MIN(a,b)	((a)<(b)?(a):(b))
#define MAX3(a,b,c)	(MAX(a,MAX(b,c)))
#define MIN3(a,b,c)	(MIN(a,MIN(b,c)))
#define CLIP(a,low,high) MAX((low),MIN((high),(a)))
#define SIZE(x)		(sizeof(x)/sizeof((x)[0]))

static int qc_sharpness = 32768;

/* Bayer-to-RGB conversion Copyright (C) Tuukka Toivonen 2003 */
/* Licensed under GPL */

/* Write RGB pixel value to the given address.
 * addr = memory address, to which the pixel is written
 * bpp = number of bytes in the pixel (should be 3 or 4)
 * r, g, b = pixel component values to be written (red, green, blue)
 * Looks horribly slow but the compiler should be able to inline optimize it.
 */
static inline void qc_imag_writergb(void *addr, int bpp,
	unsigned char r, unsigned char g, unsigned char b)
{
	if (DEFAULT_BGR) {
		/* Blue is first (in the lowest memory address */
		if (bpp==4) {
#if defined(__LITTLE_ENDIAN)
			*(unsigned int *)addr =
				(unsigned int)r << 16 |
				(unsigned int)g << 8 |
				(unsigned int)b;
#elif defined(__BIG_ENDIAN)
			*(unsigned int *)addr =
				(unsigned int)r << 8 |
				(unsigned int)g << 16 |
				(unsigned int)b << 24;
#else
			unsigned char *addr2 = (unsigned char *)addr;
			addr2[0] = b;
			addr2[1] = g;
			addr2[2] = r;
			addr2[3] = 0;
#endif
		} else {
			unsigned char *addr2 = (unsigned char *)addr;
			addr2[0] = b;
			addr2[1] = g;
			addr2[2] = r;
		}
	} else {
		/* Red is first (in the lowest memory address */
		if (bpp==4) {
#if defined(__LITTLE_ENDIAN)
			*(unsigned int *)addr =
				(unsigned int)b << 16 |
				(unsigned int)g << 8 |
				(unsigned int)r;
#elif defined(__BIG_ENDIAN)
			*(unsigned int *)addr =
				(unsigned int)b << 8 |
				(unsigned int)g << 16 |
				(unsigned int)r << 24;
#else
			unsigned char *addr2 = (unsigned char *)addr;
			addr2[0] = r;
			addr2[1] = g;
			addr2[2] = b;
			addr2[3] = 0;
#endif
		} else {
			unsigned char *addr2 = (unsigned char *)addr;
			addr2[0] = r;
			addr2[1] = g;
			addr2[2] = b;
		}
	}
}

/* Assume r, g, and b are 10-bit quantities */
static inline void qc_imag_writergb10(void *addr, int bpp,
	unsigned short r, unsigned short g, unsigned short b)
{
	qc_imag_writergb(addr, bpp, r>>2, g>>2, b>>2);
}

/* Following routines work with 8 bit RAW bayer data */

/* Convert bayer image to RGB image using fast horizontal-only interpolation.
 * bay = points to the bayer image data (upper left pixel is green)
 * bay_line = bytes between the beginnings of two consecutive rows
 * rgb = points to the rgb image data that is written
 * rgb_line = bytes between the beginnings of two consecutive rows
 * columns, rows = bayer image size (both must be even)
 * bpp = number of bytes in each pixel in the RGB image (should be 3 or 4)
 */
/* Execution time: 2735776-3095322 clock cycles for CIF image (Pentium II) */
/* Not recommended: ip seems to be somewhat faster, probably with better image quality.
 * cott is quite much faster, but possibly with slightly worse image quality */
static inline void qc_imag_bay2rgb_horip(unsigned char *bay, int bay_line,
		unsigned char *rgb, int rgb_line,
		int columns, int rows, int bpp) 
{
	unsigned char *cur_bay, *cur_rgb;
	int bay_line2, rgb_line2;
	int total_columns;
	unsigned char red, green, blue;
	unsigned int column_cnt, row_cnt;

	/* Process 2 lines and rows per each iteration */
	total_columns = (columns-2) / 2;
	row_cnt = rows / 2;
	bay_line2 = 2*bay_line;
	rgb_line2 = 2*rgb_line;
	
	do {
		qc_imag_writergb(rgb+0,        bpp, bay[1], bay[0], bay[bay_line]);
		qc_imag_writergb(rgb+rgb_line, bpp, bay[1], bay[0], bay[bay_line]);
		cur_bay = bay + 1;
		cur_rgb = rgb + bpp;
		column_cnt = total_columns;
		do {
			green = ((unsigned int)cur_bay[-1]+cur_bay[1]) / 2;
			blue  = ((unsigned int)cur_bay[bay_line-1]+cur_bay[bay_line+1]) / 2;
			qc_imag_writergb(cur_rgb+0, bpp, cur_bay[0], green, blue);
			red   = ((unsigned int)cur_bay[0]+cur_bay[2]) / 2;
			qc_imag_writergb(cur_rgb+bpp, bpp, red, cur_bay[1], cur_bay[bay_line+1]);
			green = ((unsigned int)cur_bay[bay_line]+cur_bay[bay_line+2]) / 2;
			qc_imag_writergb(cur_rgb+rgb_line, bpp, cur_bay[0], cur_bay[bay_line], blue);
			qc_imag_writergb(cur_rgb+rgb_line+bpp, bpp, red, cur_bay[1], cur_bay[bay_line+1]);
			cur_bay += 2;
			cur_rgb += 2*bpp;
		} while (--column_cnt);
		qc_imag_writergb(cur_rgb+0,        bpp, cur_bay[0], cur_bay[-1],       cur_bay[bay_line-1]);
		qc_imag_writergb(cur_rgb+rgb_line, bpp, cur_bay[0], cur_bay[bay_line], cur_bay[bay_line-1]);
		bay += bay_line2;
		rgb += rgb_line2;
	} while (--row_cnt);
}

/* Convert bayer image to RGB image using full (slow) linear interpolation.
 * bay = points to the bayer image data (upper left pixel is green)
 * bay_line = bytes between the beginnings of two consecutive rows
 * rgb = points to the rgb image data that is written
 * rgb_line = bytes between the beginnings of two consecutive rows
 * columns, rows = bayer image size (both must be even)
 * bpp = number of bytes in each pixel in the RGB image (should be 3 or 4)
 */
/* Execution time: 2714077-2827455 clock cycles for CIF image (Pentium II) */
static inline void qc_imag_bay2rgb_ip(unsigned char *bay, int bay_line,
		unsigned char *rgb, int rgb_line,
		int columns, int rows, int bpp) 
{
	unsigned char *cur_bay, *cur_rgb;
	int bay_line2, rgb_line2;
	int total_columns;
	unsigned char red, green, blue;
	unsigned int column_cnt, row_cnt;

	/* Process 2 rows and columns each iteration */
	total_columns = (columns-2) / 2;
	row_cnt = (rows-2) / 2;
	bay_line2 = 2*bay_line;
	rgb_line2 = 2*rgb_line;

	/* First scanline is handled here as a special case */	
	qc_imag_writergb(rgb, bpp, bay[1], bay[0], bay[bay_line]);
	cur_bay = bay + 1;
	cur_rgb = rgb + bpp;
	column_cnt = total_columns;
	do {
		green  = ((unsigned int)cur_bay[-1] + cur_bay[1] + cur_bay[bay_line]) / 3;
		blue   = ((unsigned int)cur_bay[bay_line-1] + cur_bay[bay_line+1]) / 2;
		qc_imag_writergb(cur_rgb, bpp, cur_bay[0], green, blue);
		red    = ((unsigned int)cur_bay[0] + cur_bay[2]) / 2;
		qc_imag_writergb(cur_rgb+bpp, bpp, red, cur_bay[1], cur_bay[bay_line+1]);
		cur_bay += 2;
		cur_rgb += 2*bpp;
	} while (--column_cnt);
	green = ((unsigned int)cur_bay[-1] + cur_bay[bay_line]) / 2;
	qc_imag_writergb(cur_rgb, bpp, cur_bay[0], green, cur_bay[bay_line-1]);

	/* Process here all other scanlines except first and last */
	bay += bay_line;
	rgb += rgb_line;
	do {
		red = ((unsigned int)bay[-bay_line+1] + bay[bay_line+1]) / 2;
		green = ((unsigned int)bay[-bay_line] + bay[1] + bay[bay_line]) / 3;
		qc_imag_writergb(rgb+0, bpp, red, green, bay[0]);
		blue = ((unsigned int)bay[0] + bay[bay_line2]) / 2;
		qc_imag_writergb(rgb+rgb_line, bpp, bay[bay_line+1], bay[bay_line], blue);
		cur_bay = bay + 1;
		cur_rgb = rgb + bpp;
		column_cnt = total_columns;
		do {
			red   = ((unsigned int)cur_bay[-bay_line]+cur_bay[bay_line]) / 2;
			blue  = ((unsigned int)cur_bay[-1]+cur_bay[1]) / 2;
			qc_imag_writergb(cur_rgb+0, bpp, red, cur_bay[0], blue);
			red   = ((unsigned int)cur_bay[-bay_line]+cur_bay[-bay_line+2]+cur_bay[bay_line]+cur_bay[bay_line+2]) / 4;
			green = ((unsigned int)cur_bay[0]+cur_bay[2]+cur_bay[-bay_line+1]+cur_bay[bay_line+1]) / 4;
			qc_imag_writergb(cur_rgb+bpp, bpp, red, green, cur_bay[1]);
			green = ((unsigned int)cur_bay[0]+cur_bay[bay_line2]+cur_bay[bay_line-1]+cur_bay[bay_line+1]) / 4;
			blue  = ((unsigned int)cur_bay[-1]+cur_bay[1]+cur_bay[bay_line2-1]+cur_bay[bay_line2+1]) / 4;
			qc_imag_writergb(cur_rgb+rgb_line, bpp, cur_bay[bay_line], green, blue);
			red   = ((unsigned int)cur_bay[bay_line]+cur_bay[bay_line+2]) / 2;
			blue  = ((unsigned int)cur_bay[1]+cur_bay[bay_line2+1]) / 2;
			qc_imag_writergb(cur_rgb+rgb_line+bpp, bpp, red, cur_bay[bay_line+1], blue);
			cur_bay += 2;
			cur_rgb += 2*bpp;
		} while (--column_cnt);
		red = ((unsigned int)cur_bay[-bay_line] + cur_bay[bay_line]) / 2;
		qc_imag_writergb(cur_rgb, bpp, red, cur_bay[0], cur_bay[-1]);
		green = ((unsigned int)cur_bay[0] + cur_bay[bay_line-1] + cur_bay[bay_line2]) / 3;
		blue = ((unsigned int)cur_bay[-1] + cur_bay[bay_line2-1]) / 2;
		qc_imag_writergb(cur_rgb+rgb_line, bpp, cur_bay[bay_line], green, blue);
		bay += bay_line2;
		rgb += rgb_line2;
	} while (--row_cnt);

	/* Last scanline is handled here as a special case */	
	green = ((unsigned int)bay[-bay_line] + bay[1]) / 2;
	qc_imag_writergb(rgb, bpp, bay[-bay_line+1], green, bay[0]);
	cur_bay = bay + 1;
	cur_rgb = rgb + bpp;
	column_cnt = total_columns;
	do {
		blue   = ((unsigned int)cur_bay[-1] + cur_bay[1]) / 2;
		qc_imag_writergb(cur_rgb, bpp, cur_bay[-bay_line], cur_bay[0], blue);
		red    = ((unsigned int)cur_bay[-bay_line] + cur_bay[-bay_line+2]) / 2;
		green  = ((unsigned int)cur_bay[0] + cur_bay[-bay_line+1] + cur_bay[2]) / 3;
		qc_imag_writergb(cur_rgb+bpp, bpp, red, green, cur_bay[1]);
		cur_bay += 2;
		cur_rgb += 2*bpp;
	} while (--column_cnt);
	qc_imag_writergb(cur_rgb, bpp, cur_bay[-bay_line], cur_bay[0], cur_bay[-1]);
}

/* Convert bayer image to RGB image using 0.5 displaced light linear interpolation.
 * bay = points to the bayer image data (upper left pixel is green)
 * bay_line = bytes between the beginnings of two consecutive rows
 * rgb = points to the rgb image data that is written
 * rgb_line = bytes between the beginnings of two consecutive rows
 * columns, rows = bayer image size (both must be even)
 * bpp = number of bytes in each pixel in the RGB image (should be 3 or 4)
 */
/* Execution time: 2167685 clock cycles for CIF image (Pentium II) */
/* Original idea for this routine from Cagdas Ogut */
static inline void qc_imag_bay2rgb_cott(unsigned char *bay, int bay_line,
		unsigned char *rgb, int rgb_line,
		int columns, int rows, int bpp)
{
	unsigned char *cur_bay, *cur_rgb;
	int bay_line2, rgb_line2;
	int total_columns;

	/* Process 2 lines and rows per each iteration, but process the last row and column separately */
	total_columns = (columns>>1) - 1;
	rows = (rows>>1) - 1;
	bay_line2 = 2*bay_line;
	rgb_line2 = 2*rgb_line;
	do {
		cur_bay = bay;
		cur_rgb = rgb;
		columns = total_columns;
		do {
			qc_imag_writergb(cur_rgb+0,            bpp, cur_bay[1],           ((unsigned int)cur_bay[0] + cur_bay[bay_line+1])          /2, cur_bay[bay_line]);
			qc_imag_writergb(cur_rgb+bpp,          bpp, cur_bay[1],           ((unsigned int)cur_bay[2] + cur_bay[bay_line+1])          /2, cur_bay[bay_line+2]);
			qc_imag_writergb(cur_rgb+rgb_line,     bpp, cur_bay[bay_line2+1], ((unsigned int)cur_bay[bay_line2] + cur_bay[bay_line+1])  /2, cur_bay[bay_line]);
			qc_imag_writergb(cur_rgb+rgb_line+bpp, bpp, cur_bay[bay_line2+1], ((unsigned int)cur_bay[bay_line2+2] + cur_bay[bay_line+1])/2, cur_bay[bay_line+2]);
			cur_bay += 2;
			cur_rgb += 2*bpp;
		} while (--columns);
		qc_imag_writergb(cur_rgb+0,            bpp, cur_bay[1], ((unsigned int)cur_bay[0] + cur_bay[bay_line+1])/2, cur_bay[bay_line]);
		qc_imag_writergb(cur_rgb+bpp,          bpp, cur_bay[1], cur_bay[bay_line+1], cur_bay[bay_line]);
		qc_imag_writergb(cur_rgb+rgb_line,     bpp, cur_bay[bay_line2+1], ((unsigned int)cur_bay[bay_line2] + cur_bay[bay_line+1])/2, cur_bay[bay_line]);
		qc_imag_writergb(cur_rgb+rgb_line+bpp, bpp, cur_bay[bay_line2+1], cur_bay[bay_line+1], cur_bay[bay_line]);
		bay += bay_line2;
		rgb += rgb_line2;
	} while (--rows);
	/* Last scanline handled here as special case */
	cur_bay = bay;
	cur_rgb = rgb;
	columns = total_columns;
	do {
		qc_imag_writergb(cur_rgb+0,            bpp, cur_bay[1], ((unsigned int)cur_bay[0] + cur_bay[bay_line+1])/2, cur_bay[bay_line]);
		qc_imag_writergb(cur_rgb+bpp,          bpp, cur_bay[1], ((unsigned int)cur_bay[2] + cur_bay[bay_line+1])/2, cur_bay[bay_line+2]);
		qc_imag_writergb(cur_rgb+rgb_line,     bpp, cur_bay[1], cur_bay[bay_line+1], cur_bay[bay_line]);
		qc_imag_writergb(cur_rgb+rgb_line+bpp, bpp, cur_bay[1], cur_bay[bay_line+1], cur_bay[bay_line+2]);
		cur_bay += 2;
		cur_rgb += 2*bpp;
	} while (--columns);
	/* Last lower-right pixel is handled here as special case */
	qc_imag_writergb(cur_rgb+0,            bpp, cur_bay[1], ((unsigned int)cur_bay[0] + cur_bay[bay_line+1])/2, cur_bay[bay_line]);
	qc_imag_writergb(cur_rgb+bpp,          bpp, cur_bay[1], cur_bay[bay_line+1], cur_bay[bay_line]);
	qc_imag_writergb(cur_rgb+rgb_line,     bpp, cur_bay[1], cur_bay[bay_line+1], cur_bay[bay_line]);
	qc_imag_writergb(cur_rgb+rgb_line+bpp, bpp, cur_bay[1], cur_bay[bay_line+1], cur_bay[bay_line]);
}

/* Convert bayer image to RGB image using 0.5 displaced nearest neighbor.
 * bay = points to the bayer image data (upper left pixel is green)
 * bay_line = bytes between the beginnings of two consecutive rows
 * rgb = points to the rgb image data that is written
 * rgb_line = bytes between the beginnings of two consecutive rows
 * columns, rows = bayer image size (both must be even)
 * bpp = number of bytes in each pixel in the RGB image (should be 3 or 4)
 */
/* Execution time: 2133302 clock cycles for CIF image (Pentium II), fastest */
static inline void qc_imag_bay2rgb_cottnoip(unsigned char *bay, int bay_line,
		unsigned char *rgb, int rgb_line,
		int columns, int rows, int bpp)
{
	unsigned char *cur_bay, *cur_rgb;
	int bay_line2, rgb_line2;
	int total_columns;

	/* Process 2 lines and rows per each iteration, but process the last row and column separately */
	total_columns = (columns>>1) - 1;
	rows = (rows>>1) - 1;
	bay_line2 = 2*bay_line;
	rgb_line2 = 2*rgb_line;
	do {
		cur_bay = bay;
		cur_rgb = rgb;
		columns = total_columns;
		do {
			qc_imag_writergb(cur_rgb+0,            bpp, cur_bay[1],           cur_bay[0], cur_bay[bay_line]);
			qc_imag_writergb(cur_rgb+bpp,          bpp, cur_bay[1],           cur_bay[2], cur_bay[bay_line+2]);
			qc_imag_writergb(cur_rgb+rgb_line,     bpp, cur_bay[bay_line2+1], cur_bay[bay_line+1], cur_bay[bay_line]);
			qc_imag_writergb(cur_rgb+rgb_line+bpp, bpp, cur_bay[bay_line2+1], cur_bay[bay_line+1], cur_bay[bay_line+2]);
			cur_bay += 2;
			cur_rgb += 2*bpp;
		} while (--columns);
		qc_imag_writergb(cur_rgb+0,            bpp, cur_bay[1], cur_bay[0], cur_bay[bay_line]);
		qc_imag_writergb(cur_rgb+bpp,          bpp, cur_bay[1], cur_bay[bay_line+1], cur_bay[bay_line]);
		qc_imag_writergb(cur_rgb+rgb_line,     bpp, cur_bay[bay_line2+1], cur_bay[bay_line+1], cur_bay[bay_line]);
		qc_imag_writergb(cur_rgb+rgb_line+bpp, bpp, cur_bay[bay_line2+1], cur_bay[bay_line+1], cur_bay[bay_line]);
		bay += bay_line2;
		rgb += rgb_line2;
	} while (--rows);
	/* Last scanline handled here as special case */
	cur_bay = bay;
	cur_rgb = rgb;
	columns = total_columns;
	do {
		qc_imag_writergb(cur_rgb+0,            bpp, cur_bay[1], cur_bay[0], cur_bay[bay_line]);
		qc_imag_writergb(cur_rgb+bpp,          bpp, cur_bay[1], cur_bay[2], cur_bay[bay_line+2]);
		qc_imag_writergb(cur_rgb+rgb_line,     bpp, cur_bay[1], cur_bay[bay_line+1], cur_bay[bay_line]);
		qc_imag_writergb(cur_rgb+rgb_line+bpp, bpp, cur_bay[1], cur_bay[bay_line+1], cur_bay[bay_line+2]);
		cur_bay += 2;
		cur_rgb += 2*bpp;
	} while (--columns);
	/* Last lower-right pixel is handled here as special case */
	qc_imag_writergb(cur_rgb+0,            bpp, cur_bay[1], cur_bay[0], cur_bay[bay_line]);
	qc_imag_writergb(cur_rgb+bpp,          bpp, cur_bay[1], cur_bay[bay_line+1], cur_bay[bay_line]);
	qc_imag_writergb(cur_rgb+rgb_line,     bpp, cur_bay[1], cur_bay[bay_line+1], cur_bay[bay_line]);
	qc_imag_writergb(cur_rgb+rgb_line+bpp, bpp, cur_bay[1], cur_bay[bay_line+1], cur_bay[bay_line]);
}

/* Convert Bayer image to RGB image using Generalized Pei-Tam method
 * Uses fixed weights */
/* Execution time: 3795517 clock cycles */
static inline void qc_imag_bay2rgb_gptm_fast(unsigned char *bay, int bay_line,
		   unsigned char *rgb, int rgb_line,
		   int columns, int rows, int bpp)
{
	int r,g,b,w;
	unsigned char *cur_bay, *cur_rgb;
	int bay_line2, bay_line3, rgb_line2;
	int total_columns;

	/* Process 2 lines and rows per each iteration, but process the first and last two columns and rows separately */
	total_columns = (columns>>1) - 2;
	rows = (rows>>1) - 2;
	bay_line2 = 2*bay_line;
	bay_line3 = 3*bay_line;
	rgb_line2 = 2*rgb_line;

	/* Process first two pixel rows here */
	cur_bay = bay;
	cur_rgb = rgb;
	columns = total_columns + 2;
	do {
		qc_imag_writergb(cur_rgb+0,            bpp, cur_bay[1], cur_bay[0],          cur_bay[bay_line]);
		qc_imag_writergb(cur_rgb+bpp,          bpp, cur_bay[1], cur_bay[0],          cur_bay[bay_line]);
		qc_imag_writergb(cur_rgb+rgb_line,     bpp, cur_bay[1], cur_bay[bay_line+1], cur_bay[bay_line]);
		qc_imag_writergb(cur_rgb+rgb_line+bpp, bpp, cur_bay[1], cur_bay[bay_line+1], cur_bay[bay_line]);
		cur_bay += 2;
		cur_rgb += 2*bpp;
	} while (--columns);
	bay += bay_line2;
	rgb += rgb_line2;

	do {
		cur_bay = bay;
		cur_rgb = rgb;
		columns = total_columns;
		
		/* Process first 2x2 pixel block in a row here */
		qc_imag_writergb(cur_rgb+0,            bpp, cur_bay[1], cur_bay[0],          cur_bay[bay_line]);
		qc_imag_writergb(cur_rgb+bpp,          bpp, cur_bay[1], cur_bay[0],          cur_bay[bay_line]);
		qc_imag_writergb(cur_rgb+rgb_line,     bpp, cur_bay[1], cur_bay[bay_line+1], cur_bay[bay_line]);
		qc_imag_writergb(cur_rgb+rgb_line+bpp, bpp, cur_bay[1], cur_bay[bay_line+1], cur_bay[bay_line]);
		cur_bay += 2;
		cur_rgb += 2*bpp;

		do {
			w = 4*cur_bay[0] - (cur_bay[-bay_line-1] + cur_bay[-bay_line+1] + cur_bay[bay_line-1] + cur_bay[bay_line+1]);
			r = (2*(cur_bay[-1] + cur_bay[1]) + w) >> 2;
			b = (2*(cur_bay[-bay_line] + cur_bay[bay_line]) + w) >> 2;
			qc_imag_writergb(cur_rgb+0, bpp, CLIP(r,0,255), cur_bay[0], CLIP(b,0,255));

			w = 4*cur_bay[1] - (cur_bay[-bay_line2+1] + cur_bay[-1] + cur_bay[3] + cur_bay[bay_line2+1]);
			g = (2*(cur_bay[-bay_line+1] + cur_bay[0] + cur_bay[2] + cur_bay[bay_line+1]) + w) >> 3;
			b = (2*(cur_bay[-bay_line] + cur_bay[-bay_line+2] + cur_bay[bay_line] + cur_bay[bay_line+2]) + w) >> 3;
			qc_imag_writergb(cur_rgb+bpp, bpp, cur_bay[1], CLIP(g,0,255), CLIP(b,0,255));

			w = 4*cur_bay[bay_line] - (cur_bay[-bay_line] + cur_bay[bay_line-2] + cur_bay[bay_line+2] + cur_bay[bay_line3]);
			r = ((cur_bay[-1] + cur_bay[1] + cur_bay[bay_line2-1] + cur_bay[bay_line2+1]) + w) >> 2;
			g = ((cur_bay[0] + cur_bay[bay_line-1] + cur_bay[bay_line+1] + cur_bay[bay_line2]) + w) >> 2;
			qc_imag_writergb(cur_rgb+rgb_line, bpp, CLIP(r,0,255), CLIP(g,0,255), cur_bay[bay_line]);

			w = 4*cur_bay[bay_line+1] - (cur_bay[0] + cur_bay[2] + cur_bay[bay_line2] + cur_bay[bay_line2+2]);
			r = (2*(cur_bay[1] + cur_bay[bay_line2+1]) + w) >> 2;
			b = (2*(cur_bay[bay_line] + cur_bay[bay_line+2]) + w) >> 2;
			qc_imag_writergb(cur_rgb+rgb_line+bpp, bpp, CLIP(r,0,255), cur_bay[bay_line+1], CLIP(b,0,255));

			cur_bay += 2;
			cur_rgb += 2*bpp;
		} while (--columns);

		/* Process last 2x2 pixel block in a row here */
		qc_imag_writergb(cur_rgb+0,            bpp, cur_bay[1], cur_bay[0],          cur_bay[bay_line]);
		qc_imag_writergb(cur_rgb+bpp,          bpp, cur_bay[1], cur_bay[0],          cur_bay[bay_line]);
		qc_imag_writergb(cur_rgb+rgb_line,     bpp, cur_bay[1], cur_bay[bay_line+1], cur_bay[bay_line]);
		qc_imag_writergb(cur_rgb+rgb_line+bpp, bpp, cur_bay[1], cur_bay[bay_line+1], cur_bay[bay_line]);

		bay += bay_line2;
		rgb += rgb_line2;
	} while (--rows);

	/* Process last two pixel rows here */
	cur_bay = bay;
	cur_rgb = rgb;
	columns = total_columns + 2;
	do {
		qc_imag_writergb(cur_rgb+0,            bpp, cur_bay[1], cur_bay[0],          cur_bay[bay_line]);
		qc_imag_writergb(cur_rgb+bpp,          bpp, cur_bay[1], cur_bay[0],          cur_bay[bay_line]);
		qc_imag_writergb(cur_rgb+rgb_line,     bpp, cur_bay[1], cur_bay[bay_line+1], cur_bay[bay_line]);
		qc_imag_writergb(cur_rgb+rgb_line+bpp, bpp, cur_bay[1], cur_bay[bay_line+1], cur_bay[bay_line]);
		cur_bay += 2;
		cur_rgb += 2*bpp;
	} while (--columns);
}

/* Convert Bayer image to RGB image using Generalized Pei-Tam method (See:
 * "Effective Color Interpolation in CCD Color Filter Arrays Using Signal Correlation"
 * IEEE Transactions on Circuits and Systems for Video Technology, vol. 13, no. 6, June 2003.
 * Note that this is much improved version of the algorithm described in the paper)
 * bay = points to the bayer image data (upper left pixel is green)
 * bay_line = bytes between the beginnings of two consecutive rows
 * rgb = points to the rgb image data that is written
 * rgb_line = bytes between the beginnings of two consecutive rows
 * columns, rows = bayer image size (both must be even)
 * bpp = number of bytes in each pixel in the RGB image (should be 3 or 4)
 * sharpness = how sharp the image should be, between 0..65535 inclusive.
 *             23170 gives in theory image that corresponds to the original
 *             best, but human eye likes slightly sharper picture... 32768 is a good bet.
 *             When sharpness = 0, this routine is same as bilinear interpolation.
 */
/* Execution time: 4344042 clock cycles for CIF image (Pentium II) */
static inline void qc_imag_bay2rgb_gptm(unsigned char *bay, int bay_line,
		   unsigned char *rgb, int rgb_line,
		   int columns, int rows, int bpp)
{

	/* 0.8 fixed point weights, should be between 0-256. Larger value = sharper, zero corresponds to bilinear interpolation. */
	/* Best PSNR with sharpness = 23170 */
	static const int wrg0 = 144;		/* Weight for Red on Green */
	static const int wbg0 = 160;
	static const int wgr0 = 120;
	static const int wbr0 = 192;
	static const int wgb0 = 120;
	static const int wrb0 = 168;

	int wrg;
	int wbg;
	int wgr;
	int wbr;
	int wgb;
	int wrb;

	unsigned int wu;
	int r,g,b,w;
	unsigned char *cur_bay, *cur_rgb;
	int bay_line2, bay_line3, rgb_line2;
	int total_columns;

	/* Compute weights */
	wu = (qc_sharpness * qc_sharpness) >> 16;
 	wu = (wu * wu) >> 16;
	wrg = (wrg0 * wu) >> 10;
	wbg = (wbg0 * wu) >> 10;
	wgr = (wgr0 * wu) >> 10;
	wbr = (wbr0 * wu) >> 10;
	wgb = (wgb0 * wu) >> 10;
	wrb = (wrb0 * wu) >> 10;

	/* Process 2 lines and rows per each iteration, but process the first and last two columns and rows separately */
	total_columns = (columns>>1) - 2;
	rows = (rows>>1) - 2;
	bay_line2 = 2*bay_line;
	bay_line3 = 3*bay_line;
	rgb_line2 = 2*rgb_line;

	/* Process first two pixel rows here */
	cur_bay = bay;
	cur_rgb = rgb;
	columns = total_columns + 2;
	do {
		qc_imag_writergb(cur_rgb+0,            bpp, cur_bay[1], cur_bay[0],          cur_bay[bay_line]);
		qc_imag_writergb(cur_rgb+bpp,          bpp, cur_bay[1], cur_bay[0],          cur_bay[bay_line]);
		qc_imag_writergb(cur_rgb+rgb_line,     bpp, cur_bay[1], cur_bay[bay_line+1], cur_bay[bay_line]);
		qc_imag_writergb(cur_rgb+rgb_line+bpp, bpp, cur_bay[1], cur_bay[bay_line+1], cur_bay[bay_line]);
		cur_bay += 2;
		cur_rgb += 2*bpp;
	} while (--columns);
	bay += bay_line2;
	rgb += rgb_line2;

	do {
		cur_bay = bay;
		cur_rgb = rgb;
		columns = total_columns;
		
		/* Process first 2x2 pixel block in a row here */
		qc_imag_writergb(cur_rgb+0,            bpp, cur_bay[1], cur_bay[0],          cur_bay[bay_line]);
		qc_imag_writergb(cur_rgb+bpp,          bpp, cur_bay[1], cur_bay[0],          cur_bay[bay_line]);
		qc_imag_writergb(cur_rgb+rgb_line,     bpp, cur_bay[1], cur_bay[bay_line+1], cur_bay[bay_line]);
		qc_imag_writergb(cur_rgb+rgb_line+bpp, bpp, cur_bay[1], cur_bay[bay_line+1], cur_bay[bay_line]);
		cur_bay += 2;
		cur_rgb += 2*bpp;

		do {
			w = 4*cur_bay[0] - (cur_bay[-bay_line-1] + cur_bay[-bay_line+1] + cur_bay[bay_line-1] + cur_bay[bay_line+1]);
			r = (512*(cur_bay[-1] + cur_bay[1]) + w*wrg) >> 10;
			b = (512*(cur_bay[-bay_line] + cur_bay[bay_line]) + w*wbg) >> 10;
			qc_imag_writergb(cur_rgb+0, bpp, CLIP(r,0,255), cur_bay[0], CLIP(b,0,255));

			w = 4*cur_bay[1] - (cur_bay[-bay_line2+1] + cur_bay[-1] + cur_bay[3] + cur_bay[bay_line2+1]);
			g = (256*(cur_bay[-bay_line+1] + cur_bay[0] + cur_bay[2] + cur_bay[bay_line+1]) + w*wgr) >> 10;
			b = (256*(cur_bay[-bay_line] + cur_bay[-bay_line+2] + cur_bay[bay_line] + cur_bay[bay_line+2]) + w*wbr) >> 10;
			qc_imag_writergb(cur_rgb+bpp, bpp, cur_bay[1], CLIP(g,0,255), CLIP(b,0,255));

			w = 4*cur_bay[bay_line] - (cur_bay[-bay_line] + cur_bay[bay_line-2] + cur_bay[bay_line+2] + cur_bay[bay_line3]);
			r = (256*(cur_bay[-1] + cur_bay[1] + cur_bay[bay_line2-1] + cur_bay[bay_line2+1]) + w*wrb) >> 10;
			g = (256*(cur_bay[0] + cur_bay[bay_line-1] + cur_bay[bay_line+1] + cur_bay[bay_line2]) + w*wgb) >> 10;
			qc_imag_writergb(cur_rgb+rgb_line, bpp, CLIP(r,0,255), CLIP(g,0,255), cur_bay[bay_line]);

			w = 4*cur_bay[bay_line+1] - (cur_bay[0] + cur_bay[2] + cur_bay[bay_line2] + cur_bay[bay_line2+2]);
			r = (512*(cur_bay[1] + cur_bay[bay_line2+1]) + w*wrg) >> 10;
			b = (512*(cur_bay[bay_line] + cur_bay[bay_line+2]) + w*wbg) >> 10;
			qc_imag_writergb(cur_rgb+rgb_line+bpp, bpp, CLIP(r,0,255), cur_bay[bay_line+1], CLIP(b,0,255));

			cur_bay += 2;
			cur_rgb += 2*bpp;
		} while (--columns);

		/* Process last 2x2 pixel block in a row here */
		qc_imag_writergb(cur_rgb+0,            bpp, cur_bay[1], cur_bay[0],          cur_bay[bay_line]);
		qc_imag_writergb(cur_rgb+bpp,          bpp, cur_bay[1], cur_bay[0],          cur_bay[bay_line]);
		qc_imag_writergb(cur_rgb+rgb_line,     bpp, cur_bay[1], cur_bay[bay_line+1], cur_bay[bay_line]);
		qc_imag_writergb(cur_rgb+rgb_line+bpp, bpp, cur_bay[1], cur_bay[bay_line+1], cur_bay[bay_line]);

		bay += bay_line2;
		rgb += rgb_line2;
	} while (--rows);

	/* Process last two pixel rows here */
	cur_bay = bay;
	cur_rgb = rgb;
	columns = total_columns + 2;
	do {
		qc_imag_writergb(cur_rgb+0,            bpp, cur_bay[1], cur_bay[0],          cur_bay[bay_line]);
		qc_imag_writergb(cur_rgb+bpp,          bpp, cur_bay[1], cur_bay[0],          cur_bay[bay_line]);
		qc_imag_writergb(cur_rgb+rgb_line,     bpp, cur_bay[1], cur_bay[bay_line+1], cur_bay[bay_line]);
		qc_imag_writergb(cur_rgb+rgb_line+bpp, bpp, cur_bay[1], cur_bay[bay_line+1], cur_bay[bay_line]);
		cur_bay += 2;
		cur_rgb += 2*bpp;
	} while (--columns);
}

/* Following routines work with 10 bit RAW bayer data (16 bits per pixel) */

/* Convert bayer image to RGB image using 0.5 displaced nearest neighbor.
 * bay = points to the bayer image data (upper left pixel is green)
 * bay_line = short ints between the beginnings of two consecutive rows
 * rgb = points to the rgb image data that is written
 * rgb_line = bytes between the beginnings of two consecutive rows
 * columns, rows = bayer image size (both must be even)
 * bpp = number of bytes in each pixel in the RGB image (should be 3 or 4)
 */
static inline void qc_imag_bay2rgb_cottnoip10(unsigned short *bay, int bay_line,
		unsigned char *rgb, int rgb_line,
		int columns, int rows, int bpp)
{
	unsigned short *cur_bay;
	unsigned char *cur_rgb;
	int bay_line2, rgb_line2;
	int total_columns;

	/* Process 2 lines and rows per each iteration, but process the last row and column separately */
	total_columns = (columns>>1) - 1;
	rows = (rows>>1) - 1;
	bay_line2 = 2*bay_line;
	rgb_line2 = 2*rgb_line;
	do {
		cur_bay = bay;
		cur_rgb = rgb;
		columns = total_columns;
		do {
			qc_imag_writergb10(cur_rgb+0,            bpp, cur_bay[1],           cur_bay[0], cur_bay[bay_line]);
			qc_imag_writergb10(cur_rgb+bpp,          bpp, cur_bay[1],           cur_bay[2], cur_bay[bay_line+2]);
			qc_imag_writergb10(cur_rgb+rgb_line,     bpp, cur_bay[bay_line2+1], cur_bay[bay_line+1], cur_bay[bay_line]);
			qc_imag_writergb10(cur_rgb+rgb_line+bpp, bpp, cur_bay[bay_line2+1], cur_bay[bay_line+1], cur_bay[bay_line+2]);
			cur_bay += 2;
			cur_rgb += 2*bpp;
		} while (--columns);
		qc_imag_writergb10(cur_rgb+0,            bpp, cur_bay[1], cur_bay[0], cur_bay[bay_line]);
		qc_imag_writergb10(cur_rgb+bpp,          bpp, cur_bay[1], cur_bay[bay_line+1], cur_bay[bay_line]);
		qc_imag_writergb10(cur_rgb+rgb_line,     bpp, cur_bay[bay_line2+1], cur_bay[bay_line+1], cur_bay[bay_line]);
		qc_imag_writergb10(cur_rgb+rgb_line+bpp, bpp, cur_bay[bay_line2+1], cur_bay[bay_line+1], cur_bay[bay_line]);
		bay += bay_line2;
		rgb += rgb_line2;
	} while (--rows);
	/* Last scanline handled here as special case */
	cur_bay = bay;
	cur_rgb = rgb;
	columns = total_columns;
	do {
		qc_imag_writergb10(cur_rgb+0,            bpp, cur_bay[1], cur_bay[0], cur_bay[bay_line]);
		qc_imag_writergb10(cur_rgb+bpp,          bpp, cur_bay[1], cur_bay[2], cur_bay[bay_line+2]);
		qc_imag_writergb10(cur_rgb+rgb_line,     bpp, cur_bay[1], cur_bay[bay_line+1], cur_bay[bay_line]);
		qc_imag_writergb10(cur_rgb+rgb_line+bpp, bpp, cur_bay[1], cur_bay[bay_line+1], cur_bay[bay_line+2]);
		cur_bay += 2;
		cur_rgb += 2*bpp;
	} while (--columns);
	/* Last lower-right pixel is handled here as special case */
	qc_imag_writergb10(cur_rgb+0,            bpp, cur_bay[1], cur_bay[0], cur_bay[bay_line]);
	qc_imag_writergb10(cur_rgb+bpp,          bpp, cur_bay[1], cur_bay[bay_line+1], cur_bay[bay_line]);
	qc_imag_writergb10(cur_rgb+rgb_line,     bpp, cur_bay[1], cur_bay[bay_line+1], cur_bay[bay_line]);
	qc_imag_writergb10(cur_rgb+rgb_line+bpp, bpp, cur_bay[1], cur_bay[bay_line+1], cur_bay[bay_line]);
}

/* Convert Bayer image to RGB image using Generalized Pei-Tam method (See:
 * "Effective Color Interpolation in CCD Color Filter Arrays Using Signal Correlation"
 * IEEE Transactions on Circuits and Systems for Video Technology, vol. 13, no. 6, June 2003.
 * Note that this is much improved version of the algorithm described in the paper)
 * bay = points to the bayer image data (upper left pixel is green)
 * bay_line = short ints between the beginnings of two consecutive rows
 * rgb = points to the rgb image data that is written
 * rgb_line = bytes between the beginnings of two consecutive rows
 * columns, rows = bayer image size (both must be even)
 * bpp = number of bytes in each pixel in the RGB image (should be 3 or 4)
 * sharpness = how sharp the image should be, between 0..65535 inclusive.
 *             23170 gives in theory image that corresponds to the original
 *             best, but human eye likes slightly sharper picture... 32768 is a good bet.
 *             When sharpness = 0, this routine is same as bilinear interpolation.
 */
/* Execution time: 4344042 clock cycles for CIF image (Pentium II) */
static inline void qc_imag_bay2rgb_gptm10(unsigned short *bay, int bay_line,
		   unsigned char *rgb, int rgb_line,
		   int columns, int rows, int bpp)
{

	/* 0.8 fixed point weights, should be between 0-256. Larger value = sharper, zero corresponds to bilinear interpolation. */
	/* Best PSNR with sharpness = 23170 */
	static const int wrg0 = 144;		/* Weight for Red on Green */
	static const int wbg0 = 160;
	static const int wgr0 = 120;
	static const int wbr0 = 192;
	static const int wgb0 = 120;
	static const int wrb0 = 168;

	int wrg;
	int wbg;
	int wgr;
	int wbr;
	int wgb;
	int wrb;

	unsigned int wu;
	int r,g,b,w;
	unsigned short *cur_bay;
	unsigned char *cur_rgb;
	int bay_line2, bay_line3, rgb_line2;
	int total_columns;

	/* Compute weights */
	wu = (qc_sharpness * qc_sharpness) >> 16;
 	wu = (wu * wu) >> 16;
	wrg = (wrg0 * wu) >> 10;
	wbg = (wbg0 * wu) >> 10;
	wgr = (wgr0 * wu) >> 10;
	wbr = (wbr0 * wu) >> 10;
	wgb = (wgb0 * wu) >> 10;
	wrb = (wrb0 * wu) >> 10;

	/* Process 2 lines and rows per each iteration, but process the first and last two columns and rows separately */
	total_columns = (columns>>1) - 2;
	rows = (rows>>1) - 2;
	bay_line2 = 2*bay_line;
	bay_line3 = 3*bay_line;
	rgb_line2 = 2*rgb_line;

	/* Process first two pixel rows here */
	cur_bay = bay;
	cur_rgb = rgb;
	columns = total_columns + 2;
	do {
		qc_imag_writergb10(cur_rgb+0,            bpp, cur_bay[1], cur_bay[0],          cur_bay[bay_line]);
		qc_imag_writergb10(cur_rgb+bpp,          bpp, cur_bay[1], cur_bay[0],          cur_bay[bay_line]);
		qc_imag_writergb10(cur_rgb+rgb_line,     bpp, cur_bay[1], cur_bay[bay_line+1], cur_bay[bay_line]);
		qc_imag_writergb10(cur_rgb+rgb_line+bpp, bpp, cur_bay[1], cur_bay[bay_line+1], cur_bay[bay_line]);
		cur_bay += 2;
		cur_rgb += 2*bpp;
	} while (--columns);
	bay += bay_line2;
	rgb += rgb_line2;

	do {
		cur_bay = bay;
		cur_rgb = rgb;
		columns = total_columns;
		
		/* Process first 2x2 pixel block in a row here */
		qc_imag_writergb10(cur_rgb+0,            bpp, cur_bay[1], cur_bay[0],          cur_bay[bay_line]);
		qc_imag_writergb10(cur_rgb+bpp,          bpp, cur_bay[1], cur_bay[0],          cur_bay[bay_line]);
		qc_imag_writergb10(cur_rgb+rgb_line,     bpp, cur_bay[1], cur_bay[bay_line+1], cur_bay[bay_line]);
		qc_imag_writergb10(cur_rgb+rgb_line+bpp, bpp, cur_bay[1], cur_bay[bay_line+1], cur_bay[bay_line]);
		cur_bay += 2;
		cur_rgb += 2*bpp;

		do {
			w = 4*cur_bay[0] - (cur_bay[-bay_line-1] + cur_bay[-bay_line+1] + cur_bay[bay_line-1] + cur_bay[bay_line+1]);
			r = (512*(cur_bay[-1] + cur_bay[1]) + w*wrg) >> 10;
			b = (512*(cur_bay[-bay_line] + cur_bay[bay_line]) + w*wbg) >> 10;
			qc_imag_writergb10(cur_rgb+0, bpp, CLIP(r,0,255), cur_bay[0], CLIP(b,0,255));

			w = 4*cur_bay[1] - (cur_bay[-bay_line2+1] + cur_bay[-1] + cur_bay[3] + cur_bay[bay_line2+1]);
			g = (256*(cur_bay[-bay_line+1] + cur_bay[0] + cur_bay[2] + cur_bay[bay_line+1]) + w*wgr) >> 10;
			b = (256*(cur_bay[-bay_line] + cur_bay[-bay_line+2] + cur_bay[bay_line] + cur_bay[bay_line+2]) + w*wbr) >> 10;
			qc_imag_writergb10(cur_rgb+bpp, bpp, cur_bay[1], CLIP(g,0,255), CLIP(b,0,255));

			w = 4*cur_bay[bay_line] - (cur_bay[-bay_line] + cur_bay[bay_line-2] + cur_bay[bay_line+2] + cur_bay[bay_line3]);
			r = (256*(cur_bay[-1] + cur_bay[1] + cur_bay[bay_line2-1] + cur_bay[bay_line2+1]) + w*wrb) >> 10;
			g = (256*(cur_bay[0] + cur_bay[bay_line-1] + cur_bay[bay_line+1] + cur_bay[bay_line2]) + w*wgb) >> 10;
			qc_imag_writergb10(cur_rgb+rgb_line, bpp, CLIP(r,0,255), CLIP(g,0,255), cur_bay[bay_line]);

			w = 4*cur_bay[bay_line+1] - (cur_bay[0] + cur_bay[2] + cur_bay[bay_line2] + cur_bay[bay_line2+2]);
			r = (512*(cur_bay[1] + cur_bay[bay_line2+1]) + w*wrg) >> 10;
			b = (512*(cur_bay[bay_line] + cur_bay[bay_line+2]) + w*wbg) >> 10;
			qc_imag_writergb10(cur_rgb+rgb_line+bpp, bpp, CLIP(r,0,255), cur_bay[bay_line+1], CLIP(b,0,255));

			cur_bay += 2;
			cur_rgb += 2*bpp;
		} while (--columns);

		/* Process last 2x2 pixel block in a row here */
		qc_imag_writergb10(cur_rgb+0,            bpp, cur_bay[1], cur_bay[0],          cur_bay[bay_line]);
		qc_imag_writergb10(cur_rgb+bpp,          bpp, cur_bay[1], cur_bay[0],          cur_bay[bay_line]);
		qc_imag_writergb10(cur_rgb+rgb_line,     bpp, cur_bay[1], cur_bay[bay_line+1], cur_bay[bay_line]);
		qc_imag_writergb10(cur_rgb+rgb_line+bpp, bpp, cur_bay[1], cur_bay[bay_line+1], cur_bay[bay_line]);

		bay += bay_line2;
		rgb += rgb_line2;
	} while (--rows);

	/* Process last two pixel rows here */
	cur_bay = bay;
	cur_rgb = rgb;
	columns = total_columns + 2;
	do {
		qc_imag_writergb10(cur_rgb+0,            bpp, cur_bay[1], cur_bay[0],          cur_bay[bay_line]);
		qc_imag_writergb10(cur_rgb+bpp,          bpp, cur_bay[1], cur_bay[0],          cur_bay[bay_line]);
		qc_imag_writergb10(cur_rgb+rgb_line,     bpp, cur_bay[1], cur_bay[bay_line+1], cur_bay[bay_line]);
		qc_imag_writergb10(cur_rgb+rgb_line+bpp, bpp, cur_bay[1], cur_bay[bay_line+1], cur_bay[bay_line]);
		cur_bay += 2;
		cur_rgb += 2*bpp;
	} while (--columns);
}

static struct {
	char *name;
	void (*algo8)(unsigned char *bay, int bay_line, unsigned char *rgb, int rgb_line, int columns, int rows, int bpp);
	void (*algo10)(unsigned short *bay, int bay_line, unsigned char *rgb, int rgb_line, int columns, int rows, int bpp);
} algorithms[] = {
	{ "horip",     qc_imag_bay2rgb_horip, NULL },
	{ "ip",        qc_imag_bay2rgb_ip, NULL },
	{ "cott",      qc_imag_bay2rgb_cott, NULL },
	{ "cottnoip",  qc_imag_bay2rgb_cottnoip, qc_imag_bay2rgb_cottnoip10 },
	{ "gptm_fast", qc_imag_bay2rgb_gptm_fast, NULL },
	{ "gptm",      qc_imag_bay2rgb_gptm, qc_imag_bay2rgb_gptm10 },
};

static void (*algo8)(unsigned char *bay, int bay_line, unsigned char *rgb, int rgb_line, int columns, int rows, int bpp)
	= qc_imag_bay2rgb_gptm;
static void (*algo10)(unsigned short *bay, int bay_line, unsigned char *rgb, int rgb_line, int columns, int rows, int bpp)
	= qc_imag_bay2rgb_cottnoip10;

/* Public interface */

void qc_imag_bay2rgb8(unsigned char *bay, int bay_line,
		unsigned char *rgb, int rgb_line,
		unsigned int columns, unsigned int rows, int bpp)
{
	if (algo8==NULL) {
		printf("No such 8-bit algorithm\n");
		exit(1);
	}
	algo8(bay, bay_line, rgb, rgb_line, columns, rows, bpp);
}

/* bay_line = image stride in the RAW data in bytes */
void qc_imag_bay2rgb10(unsigned char *bay, int bay_line,
		unsigned char *rgb, int rgb_line,
		unsigned int columns, unsigned int rows, int bpp)
{
#if DETECT_BADVAL
	int maxval = 0;
	int x, y;
#endif
	if (algo10==NULL) {
		printf("No such 10-bit algorithm\n");
		exit(1);
	}
	if ((bay_line & 1)) {
		printf("qc_imag_bay2rgb10: bayer stride must be even\n");
		exit(1);
	}
	bay_line >>= 1;
#if DETECT_BADVAL
	for (y=0; y<rows; y++) for (x=0; x<columns; x++) {
		maxval = MAX(maxval, ((unsigned short *)bay)[bay_line*y+x]);
	}
	if (maxval >= (1<<10)) printf("Warning: qc_imag_bay2rgb10: detected illegal pixel value)\n");
#endif
	algo10((unsigned short *)bay, bay_line, rgb, rgb_line, columns, rows, bpp);
}

void qc_set_sharpness(int sharpness)
{
	qc_sharpness = sharpness;
}

void qc_print_algorithms(void)
{
	int i;
	for (i=0; i<SIZE(algorithms); i++) {
		printf("\t%s (", algorithms[i].name);
		if (algorithms[i].algo8) printf("8-bit");
		if (algorithms[i].algo10) printf(",10-bit");
		printf(")\n");
	}
}

void qc_set_algorithm(const char *name)
{
	int i;
	for (i=0; i<SIZE(algorithms); i++)
		if (strcmp(name, algorithms[i].name)==0) break;
	if (i>=SIZE(algorithms)) {
		printf("No algorithm called `%s'\n", name);
		exit(1);
	}
	algo8 = algorithms[i].algo8;
	algo10 = algorithms[i].algo10;
}
