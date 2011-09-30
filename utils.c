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

#include "utils.h"

#include <stdio.h>
#include <stdarg.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <sys/ioctl.h>

extern char *progname;

/* {{{ [fold] void error(char *msg) */
void error(const char *format, ...) {
	va_list ap;
	int err;
	err = errno;
	va_start(ap, format);
	fprintf(stderr, "%s: ", progname);
	if (format)
		vfprintf(stderr, format, ap);
	else
		fprintf(stderr, "error");
	if (err) fprintf(stderr, " (%s)", strerror(err));
	fprintf(stderr, "\n");
	va_end(ap);
	if (!err)
		err = 1;
	exit(err);
}
/* }}} */
/* {{{ [fold] int xioctl(int fd, int request, void *arg) */
int xioctl(int fd, int request, void *arg)
{
	int r;
	fflush(stdout);
	r = ioctl(fd, request, arg);
	if (r == -1) error("ioctl(%i,%i,%p) error %i\n", fd, request, arg, r);
	return r;
}
/* }}} */
/* {{{ [fold] int eioctl(int fd, int request, void *arg) */
int eioctl(int fd, int request, void *arg)
{
	int r;
	fflush(stdout);
	r = ioctl(fd, request, arg);
	if (r == -1) printf("WARNING: ioctl(%i,%i,%p) error %i\n", fd, request, arg, r);
	return r;
}
/* }}} */
