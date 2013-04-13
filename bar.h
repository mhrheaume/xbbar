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

#define BAR_STATUS_SUCCESS      0
#define BAR_STATUS_BAD_NRECT    1
#define BAR_STATUS_BAD_PADDING  2
#define BAR_STATUS_BAD_XSZ      3
#define BAR_STATUS_BAD_YSZ      4
#define BAR_STATUS_BAD_FG1      5
#define BAR_STATUS_BAD_FG2      6
#define BAR_STATUS_BAD_BG       7
#define BAR_STATUS_NOMEM        8

#define MASK_NRECT     0x0001
#define MASK_PADDING   0x0002
#define MASK_RECT_XSZ  0x0004
#define MASK_RECT_YSZ  0x0008
#define MASK_FG1       0x0010
#define MASK_FG2       0x0020
#define MASK_BG        0x0040

typedef struct bar {
	Display *dpy;
	Window root;

	void *priv;
} bar_t;

typedef struct bar_attr {
	int nrect;
	int padding;
	int rect_xsz;
	int rect_ysz;

	char *fg1;
	char *fg2;
	char *bg;
} bar_attr_t;

int bar_init(unsigned int b_mask, bar_attr_t b_attr, bar_t **bar_out);

void bar_draw(bar_t *bar, int current, int max);
void bar_cleanup(bar_t *bar);

char *bar_status_tostring(int status);

#endif // __BAR_H__
