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

#define DEFAULT_FG "#3475aa"
#define DEFAULT_BG "#1a1a1a"

#define BAR_PRIV(b) (struct bar_priv*)b->priv

#define DEFAULT_UNLESS_MASKED(mask, lhs, attr) \
	if (!(mask & BAR_MASK_ ## attr)) lhs = DEFAULT_ ## attr

struct bar_priv {
	Window win;
	GC gc;

	int nrect;
	int padding;
	int rect_xsz;
	int rect_ysz;

	int xpos;
	int ypos;

	int xsz;
	int ysz;

	XColor fg;
	XColor bg;
};

static void fill_defaults(unsigned int b_mask, struct bar_attr *b_attr);
static void create_window(struct bar *bar);
static int alloc_colors(struct bar *bar, struct bar_attr *b_attr);
static int set_dimensions(struct bar *bar, struct bar_attr *b_attr);

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

static inline
float get_fill_percent(int brightness_percent, float lower, float upper)
{
	return
		brightness_percent >= upper ? 1.0 :
		brightness_percent <= lower ? 0 :
		(float)(brightness_percent - lower) / (float)(upper - lower);
}

// Creates a window and the associated graphics context
void create_window(struct bar *bar)
{
	XSetWindowAttributes wa;
	unsigned long vmask;
	int screen = DefaultScreen(bar->dpy);

	struct bar_priv *bar_p = BAR_PRIV(bar);

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

	bar_p->gc = XCreateGC(bar->dpy, bar_p->win, 0, NULL);
}

void fill_defaults(unsigned int b_mask, struct bar_attr *b_attr)
{
	DEFAULT_UNLESS_MASKED(b_mask, b_attr->nrect, NRECT);
	DEFAULT_UNLESS_MASKED(b_mask, b_attr->padding, PADDING);
	DEFAULT_UNLESS_MASKED(b_mask, b_attr->rect_xsz, RECT_XSZ);
	DEFAULT_UNLESS_MASKED(b_mask, b_attr->rect_ysz, RECT_YSZ);

	DEFAULT_UNLESS_MASKED(b_mask, b_attr->fg, FG);
	DEFAULT_UNLESS_MASKED(b_mask, b_attr->bg, BG);
}

int alloc_colors(struct bar *bar, struct bar_attr *b_attr)
{
	Colormap cmap = DefaultColormap(bar->dpy, 0);
	struct bar_priv *bar_p = BAR_PRIV(bar);
	int status;

	status = XAllocNamedColor(bar->dpy,
		cmap,
		b_attr->fg,
		&bar_p->fg,
		&bar_p->fg);

	if (!status) {
		return BAR_STATUS_BAD_FG;
	}

	status = XAllocNamedColor(bar->dpy,
		cmap,
		b_attr->bg,
		&bar_p->bg,
		&bar_p->bg);

	if (!status) {
		return BAR_STATUS_BAD_BG;
	}

	return BAR_STATUS_SUCCESS;
}

int set_dimensions(struct bar *bar, struct bar_attr *b_attr)
{
	int screen = DefaultScreen(bar->dpy);
	int screen_xsz = DisplayWidth(bar->dpy, screen);
	int screen_ysz = DisplayHeight(bar->dpy, screen);

	struct bar_priv *bar_p = BAR_PRIV(bar);

	bar_p->nrect = b_attr->nrect;
	if (b_attr->nrect < 0) {
		return BAR_STATUS_BAD_NRECT;
	}

	bar_p->padding = b_attr->padding;
	if (b_attr->padding < 0) {
		return BAR_STATUS_BAD_PADDING;
	}

	// Minimum of 5: 2 for outer box, 2 for inner box, 1 for filling
	bar_p->rect_xsz = b_attr->rect_xsz;
	if (b_attr->rect_xsz < 5) {
		return BAR_STATUS_BAD_XSZ;
	}

	// Same as above
	bar_p->rect_ysz = b_attr->rect_ysz;
	if (b_attr->rect_ysz < 5) {
		return BAR_STATUS_BAD_YSZ;
	}

	bar_p->xsz = calc_xsize(bar_p->rect_xsz, bar_p->padding, bar_p->nrect);
	bar_p->ysz = calc_ysize(bar_p->rect_ysz, bar_p->padding);

	if (bar_p->xsz > screen_xsz || bar_p->ysz > screen_ysz * 1/8) {
		return BAR_STATUS_TOO_LARGE;
	}

	bar_p->xpos = screen_xsz / 2 - (bar_p->xsz / 2);
	bar_p->ypos = screen_ysz * 15 / 16 - (bar_p->ysz / 2);

	return BAR_STATUS_SUCCESS;
}

int bar_init(unsigned int b_mask, struct bar_attr *b_attr, struct bar **bar_out)
{
	int status;
	struct bar *bar;
	struct bar_priv *bar_p;

	// TODO: b_attr being valid should probably be optional
	if (bar_out == NULL || b_attr == NULL) {
		return BAR_STATUS_BAD_PTR;
	}

	// Fill in any missing attributes with the default values
	fill_defaults(b_mask, b_attr);

	bar = malloc(sizeof(struct bar));
	if (bar == NULL) {
		return BAR_STATUS_NOMEM;
	}

	bar->priv = malloc(sizeof(struct bar_priv));
	if (bar->priv == NULL) {
		free(bar);
		return BAR_STATUS_NOMEM;
	}

	bar_p = BAR_PRIV(bar);

	bar->dpy = XOpenDisplay(NULL);
	bar->root = RootWindow(bar->dpy, 0);

	status = set_dimensions(bar, b_attr);
	if (status != BAR_STATUS_SUCCESS) {
		goto error;
	}

	status = alloc_colors(bar, b_attr);
	if (status != BAR_STATUS_SUCCESS) {
		goto error;
	}

	create_window(bar);

	XMapRaised(bar->dpy, bar_p->win);
	XFlush(bar->dpy);

	*bar_out = bar;
	return BAR_STATUS_SUCCESS;

error:
	XCloseDisplay(bar->dpy);
	free(bar_p);
	free(bar);

	return status;
}

int bar_draw(struct bar *bar, int current, int max)
{
	int i, base_x_offset, base_y_offset;
	int brightness_percent = current * 100 / max;
	struct bar_priv *bar_p;

	if (bar == NULL) {
		return BAR_STATUS_BAD_PTR;
	}

	bar_p = BAR_PRIV(bar);

	XSetForeground(bar->dpy, bar_p->gc, bar_p->fg.pixel);
	XDrawRectangle(bar->dpy,
		bar_p->win,
		bar_p->gc,
		0,
		0,
		bar_p->xsz - 1,
		bar_p->ysz - 1);

	XSetForeground(bar->dpy, bar_p->gc, bar_p->bg.pixel);
	XFillRectangle(bar->dpy,
		bar_p->win,
		bar_p->gc,
		1,
		1,
		bar_p->xsz - 2,
		bar_p->ysz - 2);

	XSetForeground(bar->dpy, bar_p->gc, bar_p->fg.pixel);

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
			bar_p->gc,
			x_offset,
			y_offset,
			bar_p->rect_xsz - 1,
			bar_p->rect_ysz - 1);

		XFillRectangle(bar->dpy,
			bar_p->win,
			bar_p->gc,
			x_offset + 2,
			y_offset + 2,
			(int)((bar_p->rect_xsz - 4) * fill_percent),
			bar_p->rect_ysz - 4);
	}

	return BAR_STATUS_SUCCESS;
}

void bar_cleanup(struct bar *bar)
{
	struct bar_priv *bar_p = BAR_PRIV(bar);

	XDestroyWindow(bar->dpy, bar_p->win);
	free(bar_p);

	XCloseDisplay(bar->dpy);
	free(bar);
}

char *bar_status_tostring(int status)
{
	char *str;

	switch (status) {
	case BAR_STATUS_SUCCESS:
		str = "success";
		break;
	case BAR_STATUS_BAD_NRECT:
		str = "bad number of rectangles";
		break;
	case BAR_STATUS_BAD_PADDING:
		str = "bad padding number";
		break;
	case BAR_STATUS_BAD_XSZ:
		str = "bad rectangle x size";
		break;
	case BAR_STATUS_BAD_YSZ:
		str = "bad rectangle y size";
		break;
	case BAR_STATUS_BAD_FG:
		str = "bad primary foreground color";
		break;
	case BAR_STATUS_BAD_BG:
		str = "bad background color";
		break;
	case BAR_STATUS_NOMEM:
		str = "out of memory";
		break;
	case BAR_STATUS_TOO_LARGE:
		str = "bar too large";
		break;
	case BAR_STATUS_BAD_PTR:
		str = "bad pointer";
		break;
	default:
		str = "unknown status";
		break;
	}

	return str;
}

