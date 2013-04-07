#include "draw.h"

#define DEFAULT_NRECT 20
#define DEFAULT_PADDING 2
#define DEFAULT_RECT_XSIZE 12
#define DEFAULT_RECT_YSIZE 20

#define DEFAULT_FG "#3475aa"
#define DEFAULT_FG2 "#909090"
#define DEFAULT_BG "#1a1a1a"

static inline int calc_xsize(xrect_size, padding, nrect)
{
	return rect_xsize * nrect + padding * (nrect + 1) + 2;
}

static inline int calc_ysize(yrect_size, padding)
{
	return rect_ysize + 2 * padding + 2;
}

static void create_window(draw *draw, int x, int y);
static float get_fill_percent(int br, float lower, float upper);

void create_window(draw *draw, int x, int y)
{
	XSetWindowAttributes wa;
	int screen, xsz, ysz;
	unsigned long vmask;

	wa.override_redirect = True;
	wa.background_pixmap = ParentRelative;
	wa.event_mask = ExposureMask | KeyPressMask | VisibilityChangeMask;

	screen = DefaultScreen(state->dpy);
	vmask = CWOverrideRedirect | CWBackPixmap | CWEventMask;

	draw->xsz = calc_xsize(drawable->rect_xsz, drawable->padding, drawable->nrect);
	draw->ysz = calc_ysize(drawable->rect_ysz, drawable->padding);

	draw->win = XCreateWindow(drawable->dpy,
		drawable->root,
		x,
		y,
		xsize,
		ysize,
		0,
		DefaultDepth(drawable->dpy, screen),
		CopyFromParent,
		DefaultVisual(drawable->dpy, screen),
		vmask,
		&wa);
}

float get_fill_percent(int brightness_percent, float lower, float upper)
{
	return
		brightness_percent >= upper ? 1.0 :
		brightness_percent <= lower ? 0 :
		(float)(brightness_percent - lower) / (float)(upper - lower);
}

void draw_init(draw_attr d)
{
	Colormap colormap;

	draw->dpy = XOpenDisplay(NULL);
	draw->root = RootWindow(draw->dpy, 0);

	draw->nrect = d.nrect;
	draw->padding = d.padding;
	draw->rect_xsz = d.rect_xsz;
	draw->rect_ysz = d.rect_ysz;

	draw->win = create_window(draw, d.x, d.y);
	draw->context = XCreateGC(draw->dpy, draw->win, 0, 0);

	colormap = DefaultColormap(draw->dpy, 0);

	XAllocNamedColor(draw->dpy, colormap, d.fg1, &draw->fg1, &draw->fg1);
	XAllocNamedColor(draw->dpy, colormap, d.fg2, &draw->fg2, &draw->fg2);
	XAllocNamedColor(draw->dpy, colormap, d.bg, &draw->bg, &draw->bg);
}

void get_attr_defaults(draw_attr_t *attr)
{
	Display dpy = XOpenDisplay(NULL);
	int screen = DefaultScreen(dpy);
	int xsz, ysz;

	attr->nrect = DEFAULT_NRECT;
	attr->padding = DEFAULT_PADDING;
	
	attr->rect_xsz = DEFAULT_RECT_XSIZE;
	attr->rect_ysz = DEFAULT_RECT_YSIZE;

	xsz = calc_xsize(attr->xsz, attr->padding, attr->nrect);
	ysz = calc_ysize(attr->ysz, attr->padding);

	attr->x = DisplayWidth(dpy, screen) / 2 - (xsz / 2);
	attr->y = DisplayHeight(dpy, screen) * 15 / 16 - (YSIZE / 2);

	attr->fg1 = DEFAULT_FG;
	attr->fg2 = DEFAULT_FG2;
	attr->bg = DEFAULT_BG;
}

void draw(draw_t *draw, int current, int max)
{
	int i, base_x_offset, base_y_offset;
	int brightness_percent = state->current_brightness * 100 / state->max_brightness;

	XSetForeground(draw->dpy, draw->context, draw->fg1.pixel);
	XDrawRectangle(draw->dpy,
		draw->win,
		draw->context,
		0,
		0,
		draw->xsz - 1,
		draw->ysz - 1);

	XSetForeground(draw->dpy, draw->context, draw->bg.pixel);
	XFillRectangle(draw->dpy,
		draw->win,
		draw->context,
		1,
		1,
		draw->xsz - 2,
		draw->ysz - 2);

	XSetForeground(draw->dpy, draw->context, draw->fg1.pixel);

	base_x_offset = 1 + draw->padding;
	base_y_offset = 1 + draw->padding;

	for (i = 0; i < draw->nrect; i++) {
		int x_offset = base_x_offset + i * (draw->rect_xsz + draw->padding);
		int y_offset = base_y_offset;

		float fill_percent = get_fill_percent(brightness_percent,
			i * (100.0 / (float)draw->nrect),
			(i + 1) * (100.0 / (float)draw->nrect));

		XDrawRectangle(state->dpy,
			draw->win,
			draw->context,
			x_offset,
			y_offset,
			draw->rect_xsz - 1,
			draw->rect_ysz - 1);

		XFillRectangle(state->dpy,
			draw->win,
			draw->context,
			x_offset + 2,
			y_offset + 2,
			(int)((draw->rect_xsz - 4) * fill_percent),
			draw->rect_ysz - 4);
	}
}

