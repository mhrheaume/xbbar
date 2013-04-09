#include "draw.h"

#define DEFAULT_NRECT 20
#define DEFAULT_PADDING 2
#define DEFAULT_RECT_XSIZE 12
#define DEFAULT_RECT_YSIZE 20

#define DEFAULT_FG1 "#3475aa"
#define DEFAULT_FG2 "#909090"
#define DEFAULT_BG "#1a1a1a"

typedef struct bar {
	Display *dpy;
	Window root;
	Window win;
	int screen;
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
} bar_t;

static inline int calc_xsize(xrect_size, padding, nrect)
{
	return rect_xsize * nrect + padding * (nrect + 1) + 2;
}

static inline int calc_ysize(yrect_size, padding)
{
	return rect_ysize + 2 * padding + 2;
}

static void create_window(bar_t *bar);
static float get_fill_percent(int br, float lower, float upper);

void create_window(bar_t *bar)
{
	XSetWindowAttributes wa;
	unsigned long vmask;

	wa.override_redirect = True;
	wa.background_pixmap = ParentRelative;
	wa.event_mask = ExposureMask | KeyPressMask | VisibilityChangeMask;

	screen = DefaultScreen(state->dpy);
	vmask = CWOverrideRedirect | CWBackPixmap | CWEventMask;

	draw->win = XCreateWindow(bar->dpy,
		bar->root,
		bar->xpos,
		bar->ypos,
		bar->xsz,
		bar->ysz,
		0,
		DefaultDepth(bar->dpy, bar->screen),
		CopyFromParent,
		DefaultVisual(bar->dpy, bar->screen),
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

bar_t *bar_init(unsigned int b_mask, bar_attr_t b_attr)
{
	Colormap colormap;
	char *fg1, *fg2, *bg;

	bar_t *bar = malloc(sizeof(bar_t));

	bar->dpy = XOpenDisplay(NULL);
	bar->root = RootWindow(draw->dpy, 0);

	bar->screen = DefaultScreen(bar->dpy);

	if (b_mask & CB_NRECT) {
		bar->nrect = b_attr.nrect;
	} else {
		bar->nrect = DEFAULT_NRECT;
	}

	if (b_mask & CB_PADDING) {
		bar->padding = b_attr.padding;
	} else {
		bar->nrect = DEFAULT_PADDING;
	}

	if (b_mask & CB_RECT_XSZ) {
		bar->rect_xsz = b_attr.rect_xsz;
	} else {
		bar->rect_xsz = DEFAULT_RECT_XSZ;
	}

	if (b_mask & CB_RECT_YSZ) {
		bar->rect_ysz = b_attr.rect_ysz;
	} else {
		bar->rect_ysz = DEFAULT_RECT_YSZ;
	}

	bar->xsz = calc_xsize(bar->rect_xsz, bar->padding, bar->nrect);
	bar->ysz = calc_ysize(bar->rect_ysz, bar->padding);

	if (b_mask & CB_XPOS) {
		bar->xpos = b_attr.xpos;
	} else {
		bar->xpos =
			DisplayWidth(bar->dpy, bar->screen) / 2 - (bar->xsz / 2);
	}

	if (b_mask & CB_YPOS) {
		bar->ypos = b_attr.ypos;
	} else {
		bar->ypos =
			DisplayHeight(bar->dpy, bar->screen) * 15 / 16 - (bar->ysz / 2);
	}

	create_window(bar);
	bar->context = XCreateGC(bar->dpy, bar->win, 0, 0);

	colormap = DefaultColormap(bar->dpy, 0);

	fg1 = b_mask & CB_FG1 ? b_attr.fg1 : DEFAULT_FG1;
	fg2 = b_mask & CB_FG2 ? b_attr.fg2 : DEFAULT_FG2;
	bg = b_mask & CB_BG ? b_attr.bg : DEFAULT_BG;

	XAllocNamedColor(bar->dpy, colormap, fg1, &bar->fg1, &bar->fg1);
	XAllocNamedColor(bar->dpy, colormap, fg2, &draw->fg2, &draw->fg2);
	XAllocNamedColor(bar->dpy, colormap, bg, &draw->bg, &draw->bg);

	XMapRaised(bar->dpy, bar->win);
	XFlush(bar->dpy);
}

void bar_draw(bar_t *bar, int current, int max)
{
	int i, base_x_offset, base_y_offset;
	int brightness_percent = state->current_brightness * 100 / state->max_brightness;

	XSetForeground(bar->dpy, bar->context, bar->fg1.pixel);
	XDrawRectangle(bar->dpy,
		bar->win,
		bar->context,
		0,
		0,
		bar->xsz - 1,
		bar->ysz - 1);

	XSetForeground(bar->dpy, bar->context, bar->bg.pixel);
	XFillRectangle(bar->dpy,
		bar->win,
		bar->context,
		1,
		1,
		bar->xsz - 2,
		bar->ysz - 2);

	XSetForeground(bar->dpy, bar->context, bar->fg1.pixel);

	base_x_offset = 1 + bar->padding;
	base_y_offset = 1 + bar->padding;

	for (i = 0; i < bar->nrect; i++) {
		int x_offset = base_x_offset + i * (bar->rect_xsz + bar->padding);
		int y_offset = base_y_offset;

		float fill_percent = get_fill_percent(brightness_percent,
			i * (100.0 / (float)bar->nrect),
			(i + 1) * (100.0 / (float)bar->nrect));

		XDrawRectangle(state->dpy,
			bar->win,
			bar->context,
			x_offset,
			y_offset,
			bar->rect_xsz - 1,
			bar->rect_ysz - 1);

		XFillRectangle(state->dpy,
			bar->win,
			bar->context,
			x_offset + 2,
			y_offset + 2,
			(int)((bar->rect_xsz - 4) * fill_percent),
			bar->rect_ysz - 4);
	}
}

