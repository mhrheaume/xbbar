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

static void write_brightness(state_t *state);
static void brightness_up(state_t *state);
static void brightness_down(state_t *state);

static void handle_kpress(state_t *state, XKeyEvent *e);
static void handle_event(state_t *state, XEvent ev);
static int grab_keyboard(state_t *state);

static void run(state_t *state);
static void cleanup(state_t *state);

static void state_init(state_t *state);

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
	draw_cleanup(state->draw);
	free(state);
}

void state_init(draw_attr_t da, state_t **state) {
	FILE *fbr, *fmax;
	state_t *new_state = malloc(sizeof(state_t));

	if (new_state == NULL) {
		fprintf(stderr, "Unable to allocate memory for state\n");
		*state = NULL;
		return;
	}

	fbr = fopen(BRIGHTNESS_CURRENT, "r");
	if (fbr == NULL) {
		fprintf(stderr, "Unable to read current brightness from %s\n",
			BRIGHTNESS_CURRENT);
		goto error;
	}

	fscanf(fbr, "%d", &state->current_brightness);
	fclose(fbr);

	fmax = fopen(BRIGHTNESS_MAX, "r");
	if (fmax == NULL) {
		fprintf(stderr, "Unable to read max brightness from %s\n",
			BRIGHTNESS_MAX);
		goto error;
	}

	fscanf(fmax, "%d", &state->max_brightness);
	fclose(fmax);

	draw_init(da, &state->drawable);

	if (state->drawable == NULL) {
		fprintf(stderr, "Failed to allocate memory for drawable\n");
		goto error;
	}

	*state = new_state;
	return;

error:
	free(new_state);
	*state = NULL;
}


int main(int argc, char **argv)
{
	unsigned int i, error;
	draw_attr_t da;
	state_t *state;

	get_attr_defaults(&da);

	for (i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "-v")) {
			printf("xbbar-"XBBAR_VERSION"\n");
			return 0;
		} else {
			fprintf(stderr, "usage: xbbar [-v]\n");
			return 1;
		}
	}

	state_init(state);

	state->running = grab_keyboard(state);
	state->error = 0;

	XMapRaised(state->draw->dpy, state->draw->win);
	XFlush(state->draw->dpy);

	run(state);

end:
	error = state->error;
	cleanup(state);
	return error;
}
