/*
 *  Copyright (C) 2011 Matthew Rheaume
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

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/XF86keysym.h>

#define DEBUG

#ifdef DEBUG
#define DEBUG_PRINTF(x) printf x
#else
#define DEBUG_PRINTF(x) do {} while (0)
#endif

#define NRECT 10
#define RECT_XSIZE 10
#define RECT_YSIZE 20

#define XSIZE (RECT_XSIZE * 20 + (NRECT - 1) + 4)
#define YSIZE (RECT_YSIZE + 4)

#define BRIGHTNESS_DIR "/sys/class/backlight/acpi_video0"
#define BRIGHTNESS_CURRENT BRIGHTNESS_DIR "/brightness"
#define BRIGHTNESS_MAX BRIGHTNESS_DIR "/max_brightness"

#define BRIGHTNESS_STEP 1

typedef struct state {
	Display *dpy;
	Window root;
	Window win;

	int current_brightness;
	int max_brightness;

	int running;
} state_t;

static Window createWindow(state_t *state);
static void draw(state_t *state);
static float get_fill_percent(int br, int lower, int upper);

static void write_brightness(state_t *state);
static void brightness_up(state_t *state);
static void brightness_down(state_t *state);

static void handle_kpress(state_t *state, XKeyEvent *e);
static void handle_event(state_t *staet, XEvent ev);

static void run(state_t *state);
static void cleanup(state_t *state);

Window createWindow(state_t *state)
{
	XSetWindowAttributes wa;
	int screen, x, y;
	unsigned long vmask;

	wa.override_redirect = True;
	wa.background_pixmap = ParentRelative;
	wa.event_mask = ExposureMask | KeyPressMask | VisibilityChangeMask;

	screen = DefaultScreen(state->dpy);
	vmask = CWOverrideRedirect | CWBackPixmap | CWEventMask;

	x = DisplayWidth(state->dpy, screen) / 2 - (XSIZE / 2);
	y = DisplayHeight(state->dpy, screen) * 15 / 16 - (YSIZE / 2);

	return
	XCreateWindow(state->dpy,
		state->root,
		x,
		y,
		XSIZE,
		YSIZE,
		0,
		DefaultDepth(state->dpy, screen),
		CopyFromParent,
		DefaultVisual(state->dpy, screen),
		vmask,
		&wa);
}

void draw(state_t *state)
{
	GC context;
	Colormap colormap;

	XColor xcolor_fg1, xcolor_fg2, xcolor_bg;
	char *color_fg1 = "#3475aa";
	char *color_fg2 = "#909090";
	char *color_bg = "#1a1a1a";

	int i, base_x_offset, base_y_offset;

	int brightness_percent = state->current_brightness * 100 / state->max_brightness;

	colormap = DefaultColormap(state->dpy, 0);
	context = XCreateGC(state->dpy, state->win, 0, 0);

	XParseColor(state->dpy, colormap, color_fg1, &xcolor_fg1);
	XAllocColor(state->dpy, colormap, &xcolor_fg1);

	XParseColor(state->dpy, colormap, color_fg2, &xcolor_fg2);
	XAllocColor(state->dpy, colormap, &xcolor_fg2);

	XParseColor(state->dpy, colormap, color_bg, &xcolor_bg);
	XAllocColor(state->dpy, colormap, &xcolor_bg);

	XSetForeground(state->dpy, context, xcolor_fg2.pixel);
	XDrawRectangle(state->dpy, state->win, context, 0, 0, XSIZE - 1, YSIZE - 1);
	XSetForeground(state->dpy, context, xcolor_bg.pixel);
	XFillRectangle(state->dpy, state->win, context, 1, 1, XSIZE - 2, YSIZE - 2);

	XSetForeground(state->dpy, context, xcolor_fg1.pixel);

	base_x_offset = 2;
	base_y_offset = 2;

	for (i = 0; i < NRECT; i++) {
		int x_offset = base_x_offset + i * (RECT_XSIZE + 1);
		int y_offset = base_y_offset;

		float fill_percent = get_fill_percent(brightness_percent,
			i * (100 / NRECT),
			(i + 1) * (100 / NRECT));

		XDrawRectangle(state->dpy,
			state->win,
			context,
			x_offset,
			y_offset,
			RECT_XSIZE - 1,
			RECT_YSIZE - 1);

		XFillRectangle(state->dpy,
			state->win,
			context,
			x_offset + 2,
			y_offset + 2,
			(int)((RECT_XSIZE - 4) * fill_percent),
			RECT_YSIZE - 4);
	}
}

float get_fill_percent(int brightness_percent, int lower, int upper) {
	// Return a number between 0 and 1, representing the percentage of the
	// rectangle to be filled.
	return
		brightness_percent >= upper ? 1.0 :
		brightness_percent <= lower ? 0 :
		(float)(brightness_percent - lower) / (float)(upper - lower);
}

void write_brightness(state_t *state)
{
	FILE *fbr = fopen(BRIGHTNESS_DIR "/brightness", "w");
	if (fbr == NULL) {
		fprintf(stderr, "Unable to write current brightness to %s",
			BRIGHTNESS_MAX);

		state->running = 0;
		return;
	}

	DEBUG_PRINTF(("current: %d; max: %d; perc: %d",
		state->current_brightness,
		state->max_brightness,
		state->current_brightness * 100 / state->max_brightness));

	fprintf(fbr, "%d", state->current_brightness);
	fclose(fbr);
}

void brightness_up(state_t *state)
{
	state->current_brightness += BRIGHTNESS_STEP;
	if (state->current_brightness > state->max_brightness) {
		state->current_brightness = state->max_brightness;
	}

	write_brightness(state);
}

void brightness_down(state_t *state)
{
	state->current_brightness -= BRIGHTNESS_STEP;
	if (state->current_brightness < 0) {
		state->current_brightness = 0;
	}

	write_brightness(state);
}


void handle_kpress(state_t *state, XKeyEvent *e)
{
	KeySym sym;

	XLookupString(e, NULL, 0, &sym, NULL);
	switch (sym) {
	case XF86XK_MonBrightnessUp:
		brightness_up(state);
		// draw(state);
		break;
	case XF86XK_MonBrightnessDown:
		brightness_down(state);
		// draw(state);
		break;
	}
}

void handle_event(state_t *state, XEvent ev)
{
	switch (ev.type) {
	case Expose:
		if (ev.xexpose.count == 0) {
			draw(state);
		}
		break;
	case KeyPress:
		handle_kpress(state, &ev.xkey);
		break;
	case VisibilityNotify:
	default:
		break;
	}
}

void run(state_t *state)
{
	XEvent ev;

	while (state->running && !XNextEvent(state->dpy, &ev)) {
		handle_event(state, ev);
	}
}

void cleanup(state_t *state)
{
	XDestroyWindow(state->dpy, state->win);
	free(state);
}

int main(int argc, char **argv)
{
	state_t *state = malloc(sizeof(state_t));

	FILE *fbr, *fmax;

	fbr = fopen(BRIGHTNESS_CURRENT, "r");
	if (fbr == NULL) {
		fprintf(stderr, "Unable to read current brightness from %s.",
			BRIGHTNESS_CURRENT);
		goto end;
	}

	fscanf(fbr, "%d", &state->current_brightness);
	fclose(fbr);

	fmax = fopen(BRIGHTNESS_MAX, "r");
	if (fmax == NULL) {
		fprintf(stderr, "Unable to read max brightness from %s.",
			BRIGHTNESS_MAX);
		goto end;
	}

	fscanf(fmax, "%d", &state->max_brightness);
	fclose(fmax);

	state->dpy = XOpenDisplay(NULL);
	state->root = RootWindow(state->dpy, 0);
	state->win = createWindow(state);

	state->running = 1;

	XMapRaised(state->dpy, state->win);
	XFlush(state->dpy);

	run(state);

end:
	cleanup(state);
	return 0;
}
