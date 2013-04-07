/*
 *  Copyright (C) 2013 Matthew Rheaume
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef __DRAW_H__
#define __DRAW_H__

typedef struct draw {
	Display *dpy;
	Window root;
	Window win;
	GC context;

	int nrect;
	int padding;
	int rect_xsz;
	int rect_ysz;

	int xsz;
	int ysz;

	XColor fg1;
	XColor fg2;
	XColor bg;
} draw_t;

typedef struct draw_attr {
	int nrect;
	int padding;

	int x;
	int y;
	int rect_xsz;
	int rect_ysz;

	char *fg1;
	char *fg2;
	char *bg;
} draw_attr_t;

void draw_init(draw_t *drawable);
void draw(draw_t *drawable, int current, int max);

void get_attr_defaults(draw_attr_t *attr);
