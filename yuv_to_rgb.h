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

/*
 *  Simple YUV-to-RGB color conversion routine
 *
 *  Author: Tuukka Toivonen <tuukka.o.toivonen@nokia.com>
 */

#ifndef YUV_TO_RGB_H
#define YUV_TO_RGB_H

#ifndef RGBSHIFT
#define RGBSHIFT		8
#endif

#ifndef MAX
#define MAX(a,b)		((a)>(b)?(a):(b))
#endif
#ifndef MIN
#define MIN(a,b)		((a)<(b)?(a):(b))
#endif
#ifndef CLAMP
#define CLAMP(a,low,high)	MAX((low),MIN((high),(a)))
#endif
#ifndef CLIP
#define CLIP(x)			CLAMP(x,0,255)
#endif

static inline void yuv_to_rgb(int y, int u, int v, int *r, int *g, int *b)
{
#if 0
	/* Video demystified; JFIF color space (http://www.fourcc.org/fccyvrgb.php) */
	int y256 = y<<8;
	int cr = u - 128;
	int cb = v - 128;
	*r = CLIP((y256 + 359*cr         ) >> RGBSHIFT);
	*g = CLIP((y256 - 183*cr -  88*cb) >> RGBSHIFT);
	*b = CLIP((y256          + 454*cb) >> RGBSHIFT);
#elif 0
	/* from TI ISP TRM: BT.601-5 range[0:255] */
	int c = y;
	int d = u - 128;	/* = Cr */
	int e = v - 128;	/* = Cb */
	*r = CLIP(( 256 * c + 351 * d   + 0 * e) >> RGBSHIFT);
	*g = CLIP(( 256 * c - 179 * d  - 86 * e) >> RGBSHIFT);
	*b = CLIP(( 256 * c   + 0 * d + 443 * e) >> RGBSHIFT);
#elif 1
	/* Conversion from
	 * http://msdn.microsoft.com/library/default.asp?url=/library/en-us/wceddraw/html/_dxce_converting_between_yuv_and_rgb.asp */
	/* Also from Wikipedia: http://en.wikipedia.org/wiki/YUV */
	int c = y - 16;
	int d = u - 128;
	int e = v - 128;
	/* Using the previous coefficients and noting that clip() denotes clipping a value to the range of 0 to 255,
	 * the following formulas provide the conversion from YUV to RGB: */
	*r = CLIP(( 298 * c           + 409 * e + 128) >> RGBSHIFT);
	*g = CLIP(( 298 * c - 100 * d - 208 * e + 128) >> RGBSHIFT);
	*b = CLIP(( 298 * c + 516 * d           + 128) >> RGBSHIFT);
#elif 0
	/* Conversion from
	 * http://www.cs.brandeis.edu/~cs155/YUV_to_RGB.c */
	/* Bad colors, otherwise fine */
//
//	Fixed point defines
//
#define YUV_SHIFT	(RGBSHIFT+8)
#define YUV_ONE		(1 << YUV_SHIFT)
#define YUV_HALF	(1 << (YUV_SHIFT - 1))

#define C_1		(-91882)	//   ((int)(-1.40200 * YUV_ONE))
#define C_2		(-22554)	//   ((int)(-0.34414 * YUV_ONE))
#define C_3		(46801)		//   ((int)( 0.71414 * YUV_ONE))
#define C_4		(116129)	//   ((int)( 1.77200 * YUV_ONE))

	u -= 127;
	v -= 127;
	*r = CLIP(y + ((C_1 * v + YUV_HALF) >> YUV_SHIFT));
	*g = CLIP(y + ((C_2 * u + C_3 * v + YUV_HALF) >> YUV_SHIFT));
	*b = CLIP(y + ((C_4 * u + YUV_HALF ) >> YUV_SHIFT));

#else
	/* Hämy conversion */
	*r = u;
	*g = y;
	*b = v;
#endif
}

#endif
