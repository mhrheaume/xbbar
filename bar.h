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

#ifndef BAR_H
#define BAR_H

#define BAR_STATUS_SUCCESS      0
#define BAR_STATUS_BAD_NRECT    1
#define BAR_STATUS_BAD_PADDING  2
#define BAR_STATUS_BAD_XSZ      3
#define BAR_STATUS_BAD_YSZ      4
#define BAR_STATUS_BAD_FG       5
#define BAR_STATUS_BAD_BG       7
#define BAR_STATUS_NOMEM        8
#define BAR_STATUS_TOO_LARGE    9
#define BAR_STATUS_BAD_PTR     10

#define BAR_MASK_NRECT     0x0001
#define BAR_MASK_PADDING   0x0002
#define BAR_MASK_RECT_XSZ  0x0004
#define BAR_MASK_RECT_YSZ  0x0008
#define BAR_MASK_FG        0x0010
#define BAR_MASK_BG        0x0040

struct bar {
	Display *dpy;
	Window root;

	void *priv;
};

struct bar_attr {
	int nrect;
	int padding;
	int rect_xsz;
	int rect_ysz;

	char *fg;
	char *bg;
};

int bar_init(unsigned int mask,
	struct bar_attr *attr,
	struct bar **bar_out);

int bar_draw(struct bar *bar, int current, int max);
void bar_cleanup(struct bar *bar);

char *bar_status_tostring(int status);

#endif // BAR_H
