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

#include <stdlib.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include "bar.h"

#define DEFAULT_NRECT 20
#define DEFAULT_PADDING 2
#define DEFAULT_RECT_XSZ 12
#define DEFAULT_RECT_YSZ 20

#define DEFAULT_FG1 "#3475aa"
#define DEFAULT_FG2 "#909090"
#define DEFAULT_BG "#1a1a1a"

#define BAR_PRIV(b) (bar_priv_t*)b->priv

#define DEFAULT_UNLESS_MASKED(mask, lhs, attr) \
	if (!(mask & MASK_ ## attr)) lhs = DEFAULT_ ## attr

typedef struct bar_priv {
	Window win;

	GC context;

	int nrect;
	int padding;
	int rect_xsz;
	int rect_ysz;

	int xpos;
	int ypos;

	int xsz;
	int ysz;

	XColor fg1;
	XColor fg2;
	XColor bg;
} bar_priv_t;

static void create_window(bar_t *bar);
static void fill_defaults(unsigned int b_mask, bar_attr_t *b_attr);

__attribute__((always_inline))
static inline int calc_xsize(rect_xsz, padding, nrect)
{
	return rect_xsz * nrect + padding * (nrect + 1) + 2;
}

__attribute__((always_inline))
static inline int calc_ysize(rect_ysz, padding)
{
	return rect_ysz + 2 * padding + 2;
}

static inline float get_fill_percent(int brightness_percent, float lower, float upper)
{
	return
		brightness_percent >= upper ? 1.0 :
		brightness_percent <= lower ? 0 :
		(float)(brightness_percent - lower) / (float)(upper - lower);
}

void create_window(bar_t *bar)
{
	XSetWindowAttributes wa;
	unsigned long vmask;
	int screen = DefaultScreen(bar->dpy);

	bar_priv_t *bar_p = BAR_PRIV(bar);

	wa.override_redirect = True;
	wa.background_pixmap = ParentRelative;
	wa.event_mask = ExposureMask | KeyPressMask | VisibilityChangeMask;

	vmask = CWOverrideRedirect | CWBackPixmap | CWEventMask;

	bar_p->win = XCreateWindow(bar->dpy,
		bar->root,
		bar_p->xpos,
		bar_p->ypos,
		bar_p->xsz,
		bar_p->ysz,
		0,
		DefaultDepth(bar->dpy, screen),
		CopyFromParent,
		DefaultVisual(bar->dpy, screen),
		vmask,
		&wa);
}

void fill_defaults(unsigned int b_mask, bar_attr_t *b_attr)
{
	DEFAULT_UNLESS_MASKED(b_mask, b_attr->nrect, NRECT);
	DEFAULT_UNLESS_MASKED(b_mask, b_attr->padding, PADDING);
	DEFAULT_UNLESS_MASKED(b_mask, b_attr->rect_xsz, RECT_XSZ);
	DEFAULT_UNLESS_MASKED(b_mask, b_attr->rect_ysz, RECT_YSZ);

	DEFAULT_UNLESS_MASKED(b_mask, b_attr->fg1, FG1);
	DEFAULT_UNLESS_MASKED(b_mask, b_attr->fg2, FG2);
	DEFAULT_UNLESS_MASKED(b_mask, b_attr->bg, BG);
}

bar_t *bar_init(unsigned int b_mask, bar_attr_t b_attr)
{
	Colormap cmap;
	int screen;

	bar_t *bar;
	bar_priv_t *bar_p;
	
	bar = malloc(sizeof(bar_t));
	if (bar == NULL) {
		return NULL;
	}

	bar->priv = malloc(sizeof(bar_priv_t));
	if (bar->priv == NULL) {
		free(bar);
		return NULL;
	}

	bar_p = BAR_PRIV(bar);

	bar->dpy = XOpenDisplay(NULL);
	bar->root = RootWindow(bar->dpy, 0);

	fill_defaults(b_mask, &b_attr);

	bar_p->nrect = b_attr.nrect;
	bar_p->padding = b_attr.padding;
	bar_p->rect_xsz = b_attr.rect_xsz;
	bar_p->rect_ysz = b_attr.rect_ysz;

	cmap = DefaultColormap(bar->dpy, 0);

	XAllocNamedColor(bar->dpy, cmap, b_attr.fg1, &bar_p->fg1, &bar_p->fg1);
	XAllocNamedColor(bar->dpy, cmap, b_attr.fg2, &bar_p->fg2, &bar_p->fg2);
	XAllocNamedColor(bar->dpy, cmap, b_attr.bg, &bar_p->bg, &bar_p->bg);

	bar_p->xsz = calc_xsize(bar_p->rect_xsz, bar_p->padding, bar_p->nrect);
	bar_p->ysz = calc_ysize(bar_p->rect_ysz, bar_p->padding);

	screen = DefaultScreen(bar->dpy);

	bar_p->xpos = DisplayWidth(bar->dpy, screen) / 2 - (bar_p->xsz / 2);
	bar_p->ypos = DisplayHeight(bar->dpy, screen) * 15 / 16 - (bar_p->ysz / 2);

	create_window(bar);

	bar_p->context = XCreateGC(bar->dpy, bar_p->win, 0, 0);

	XMapRaised(bar->dpy, bar_p->win);
	XFlush(bar->dpy);

	return bar;
}

void bar_draw(bar_t *bar, int current, int max)
{
	int i, base_x_offset, base_y_offset;
	int brightness_percent = current * 100 / max;

	bar_priv_t *bar_p = BAR_PRIV(bar);

	XSetForeground(bar->dpy, bar_p->context, bar_p->fg1.pixel);
	XDrawRectangle(bar->dpy,
		bar_p->win,
		bar_p->context,
		0,
		0,
		bar_p->xsz - 1,
		bar_p->ysz - 1);

	XSetForeground(bar->dpy, bar_p->context, bar_p->bg.pixel);
	XFillRectangle(bar->dpy,
		bar_p->win,
		bar_p->context,
		1,
		1,
		bar_p->xsz - 2,
		bar_p->ysz - 2);

	XSetForeground(bar->dpy, bar_p->context, bar_p->fg1.pixel);

	base_x_offset = 1 + bar_p->padding;
	base_y_offset = 1 + bar_p->padding;

	for (i = 0; i < bar_p->nrect; i++) {
		int x_offset = base_x_offset + i * (bar_p->rect_xsz + bar_p->padding);
		int y_offset = base_y_offset;

		float fill_percent = get_fill_percent(brightness_percent,
			i * (100.0 / (float)bar_p->nrect),
			(i + 1) * (100.0 / (float)bar_p->nrect));

		XDrawRectangle(bar->dpy,
			bar_p->win,
			bar_p->context,
			x_offset,
			y_offset,
			bar_p->rect_xsz - 1,
			bar_p->rect_ysz - 1);

		XFillRectangle(bar->dpy,
			bar_p->win,
			bar_p->context,
			x_offset + 2,
			y_offset + 2,
			(int)((bar_p->rect_xsz - 4) * fill_percent),
			bar_p->rect_ysz - 4);
	}
}

void bar_cleanup(bar_t *bar)
{
	bar_priv_t *bar_p = BAR_PRIV(bar);

	XDestroyWindow(bar->dpy, bar_p->win);
	free(bar_p);

	XCloseDisplay(bar->dpy);
	free(bar);
}
