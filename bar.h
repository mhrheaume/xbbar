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

#ifndef __BAR_H__
#define __BAR_H__

#define CB_NRECT     0x0001
#define CB_PADDING   0x0002
#define CB_XPOS      0x0004
#define CB_YPOS      0x0008
#define CB_RECT_XSZ  0x0010
#define CB_RECT_YSZ  0x0020
#define CB_FG1       0x0040
#define CB_FG2       0x0080
#define CB_BG        0x0100

typedef struct bar bar_t;

typedef struct bar_attr {
	int nrect;
	int padding;

	int xpos;
	int ypos;
	int rect_xsz;
	int rect_ysz;

	char *fg1;
	char *fg2;
	char *bg;
} bar_attr_t;

bar_t *bar_init(unsigned int bar_mask, bar_attr_t bar_attr);
void bar_draw(bar_t *drawable, int current, int max);
void bar_cleanup(bar_t *drawable);

#endif // __BAR_H__
