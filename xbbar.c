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

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <unistd.h>
#include <string.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/XF86keysym.h>

#include "version.h"

#ifdef DEBUG
#define DEBUG_PRINTF(x) printf x
#else
#define DEBUG_PRINTF(x) do {} while (0)
#endif

#define NRECT 20
#define PADDING 2
#define RECT_XSIZE 12
#define RECT_YSIZE 20

#define XSIZE (RECT_XSIZE * NRECT + PADDING * (NRECT - 1) + 2 * PADDING + 2)
#define YSIZE (RECT_YSIZE + 2 * PADDING + 2)

#define BRIGHTNESS_DIR "/sys/class/backlight/acpi_video0"
#define BRIGHTNESS_CURRENT BRIGHTNESS_DIR "/brightness"
#define BRIGHTNESS_MAX BRIGHTNESS_DIR "/max_brightness"

#define BRIGHTNESS_STEP 1

typedef struct state {
	int current_brightness;
	int max_brightness;

	int running;
	int error;

	draw_t *drawable;
} state_t;

static Window create_window(state_t *state);
static void draw(state_t *state);
static float get_fill_percent(int br, float lower, float upper);

static void write_brightness(state_t *state);
static void brightness_up(state_t *state);
static void brightness_down(state_t *state);

static void handle_kpress(state_t *state, XKeyEvent *e);
static void handle_event(state_t *state, XEvent ev);
static int grab_keyboard(state_t *state);

static void run(state_t *state);
static void cleanup(state_t *state);

Window create_window(state_t *state)
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

	base_x_offset = 1 + PADDING;
	base_y_offset = 1 + PADDING;

	for (i = 0; i < NRECT; i++) {
		int x_offset = base_x_offset + i * (RECT_XSIZE + PADDING);
		int y_offset = base_y_offset;

		float fill_percent = get_fill_percent(brightness_percent,
			i * (100.0 / (float)NRECT),
			(i + 1) * (100.0 / (float)NRECT));

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

float get_fill_percent(int brightness_percent, float lower, float upper)
{
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
		fprintf(stderr, "Unable to write current brightness to %s.\n",
			BRIGHTNESS_MAX);

		state->running = 0;
		state->error = 1;
		return;
	}

	DEBUG_PRINTF(("current: %d; max: %d; perc: %d\n",
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
	case XK_Up:
	case XK_K:
	case XK_k:
		brightness_up(state);
		draw(state);
		break;
	case XF86XK_MonBrightnessDown:
	case XK_Down:
	case XK_J:
	case XK_j:
		brightness_down(state);
		draw(state);
		break;
	case XK_Escape:
		state->running = 0;
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

int grab_keyboard(state_t *state)
{
	unsigned int len;

	for (len = 100; len; len--) {
		int result = XGrabKeyboard(state->dpy,
			state->root,
			True,
			GrabModeAsync,
			GrabModeAsync,
			CurrentTime);

		if (result == GrabSuccess) {
			break;
		}

		usleep(1000);
	}

	return len > 0;
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
	XCloseDisplay(state->dpy);
	free(state);
}

void state_init(state_t *state) {
	FILE *fbr, *fmax;

	fbr = fopen(BRIGHTNESS_CURRENT, "r");
	if (fbr == NULL) {
		fprintf(stderr, "Unable to read current brightness from %s.\n",
			BRIGHTNESS_CURRENT);
		goto end;
	}

	fscanf(fbr, "%d", &state->current_brightness);
	fclose(fbr);

	fmax = fopen(BRIGHTNESS_MAX, "r");
	if (fmax == NULL) {
		fprintf(stderr, "Unable to read max brightness from %s.\n",
			BRIGHTNESS_MAX);
		goto end;
	}

	fscanf(fmax, "%d", &state->max_brightness);
	fclose(fmax);

	state->drawable = malloc(sizeof(draw_t));
	draw_init(state->drawable);
}


int main(int argc, char **argv)
{
	unsigned int i, error;

	state_t *state = malloc(sizeof(state_t));
	state_init(state);

	for (i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "-v")) {
			printf("xbbar-"XBBAR_VERSION"\n");
			state->error = 0;
			goto end;
		} else {
			fprintf(stderr, "usage: xbbar [-v]\n");
			state->error = 1;
			goto end;
		}
	}

	state->dpy = XOpenDisplay(NULL);
	state->root = RootWindow(state->dpy, 0);
	state->win = create_window(state);

	state->running = grab_keyboard(state);
	state->error = 0;

	XMapRaised(state->dpy, state->win);
	XFlush(state->dpy);

	run(state);

end:
	error = state->error;
	cleanup(state);
	return error;
}
