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


void qc_imag_bay2rgb8(unsigned char *bay, int bay_line,
		unsigned char *rgb, int rgb_line,
		unsigned int columns, unsigned int rows, int bpp);

void qc_imag_bay2rgb10(unsigned char *bay, int bay_line,
		unsigned char *rgb, int rgb_line,
		unsigned int columns, unsigned int rows, int bpp);

void qc_set_sharpness(int sharpness);

void qc_print_algorithms(void);

void qc_set_algorithm(const char *name);
