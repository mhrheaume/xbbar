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

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/XF86keysym.h>

#include "version.h"
#include "bar.h"

#ifdef DEBUG
#define DEBUG_PRINTF(x) printf x
#else
#define DEBUG_PRINTF(x) do {} while (0)
#endif

#define EPRINTF(...) fprintf(stderr, __VA_ARGS__)

#define BRIGHTNESS_DIR "/sys/class/backlight/acpi_video0"
#define BRIGHTNESS_CURRENT BRIGHTNESS_DIR "/brightness"
#define BRIGHTNESS_MAX BRIGHTNESS_DIR "/max_brightness"

#define BRIGHTNESS_STEP 1

typedef struct state {
	int current_brightness;
	int max_brightness;

	int running;
	int error;

	bar_t *bar;
} state_t;

static void usage();
static int parse_positive_int(char *str);
static int parse_args(int argc,
	char **argv,
	bar_attr_t *b_attr,
	unsigned int *b_mask);

static void write_brightness(state_t *state);
static void brightness_up(state_t *state);
static void brightness_down(state_t *state);

static void handle_kpress(state_t *state, XKeyEvent *e);
static void handle_event(state_t *state, XEvent ev);
static int grab_keyboard(state_t *state);

static void run(state_t *state);
static void cleanup(state_t *state);

static state_t *state_init(unsigned int b_mask, bar_attr_t b_attr);

void usage()
{
	EPRINTF(
		"usage: xbbar [-v] [-h] [-p <padding>] [-n <nrect>] [-xs <rect_xsz>]\n"
		"             [-ys <rect_ysz>]\n");
}

// Returns the integer if >= 0, -1 otherwise
int parse_positive_int(char *str)
{
	int ret;

	errno = 0;
	ret = (int)strtol(str, NULL, 10);

	if (errno == EINVAL || errno == ERANGE) {
		return -1;
	}

	return ret < 0 ? -1 : ret;
}

int parse_args(int argc, char **argv, bar_attr_t *b_attr, unsigned int *b_mask)
{
	unsigned int i;

	for (i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "-v")) {
			printf("xbbar-"XBBAR_VERSION"\n");
			return 0;
		} else if (!strcmp(argv[i], "-p")) {
			if (!(++i < argc)) {
				EPRINTF("-p: missing argument\n");
				return 0;
			}

			b_attr->padding = parse_positive_int(argv[i]);
			*b_mask |= MASK_PADDING;

			if (b_attr->padding < 0) {
				EPRINTF("-p: invalid argument\n");
				return 0;
			}
		} else if (!strcmp(argv[i], "-n")) {
			if (!(++i < argc)) {
				EPRINTF("-n: missing argument\n");
				return 0;
			}

			b_attr->nrect = parse_positive_int(argv[i]);
			*b_mask |= MASK_NRECT;

			if (b_attr->nrect < 0) {
				EPRINTF("-n: invalid argument\n");
				return 0;
			}
		} else if (!strcmp(argv[i], "-xs")) {
			if (!(++i < argc)) {
				EPRINTF("-xs: missing argument\n");
				return 0;
			}

			b_attr->rect_xsz = parse_positive_int(argv[i]);
			*b_mask |= MASK_RECT_XSZ;

			if (b_attr->rect_xsz < 0) {
				EPRINTF("-xs: invalid argument\n");
				return 0;
			}
		} else if (!strcmp(argv[i], "-ys")) {
			if (!(++i < argc)) {
				EPRINTF("-ys: missing argument\n");
				return 0;
			}

			b_attr->rect_ysz = parse_positive_int(argv[i]);
			*b_mask |= MASK_RECT_YSZ;

			if (b_attr->rect_ysz < 0) {
				EPRINTF("-ys: invalid argument\n");
				return 0;
			}
		} else {
			usage();
			return 0;
		}
	}

	return 1;
}

void write_brightness(state_t *state)
{
	FILE *fbr = fopen(BRIGHTNESS_DIR "/brightness", "w");
	if (fbr == NULL) {
		EPRINTF("Unable to write current brightness to %s.\n", BRIGHTNESS_MAX);

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
		bar_draw(state->bar, state->current_brightness, state->max_brightness);
		break;
	case XF86XK_MonBrightnessDown:
	case XK_Down:
	case XK_J:
	case XK_j:
		brightness_down(state);
		bar_draw(state->bar, state->current_brightness, state->max_brightness);
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
			bar_draw(state->bar,
				state->current_brightness,
				state->max_brightness);
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
		int result = XGrabKeyboard(state->bar->dpy,
			state->bar->root,
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

	while (state->running && !XNextEvent(state->bar->dpy, &ev)) {
		handle_event(state, ev);
	}
}

void cleanup(state_t *state)
{
	bar_cleanup(state->bar);
	free(state);
}

state_t *state_init(unsigned int b_mask, bar_attr_t b_attr) {
	FILE *fbr, *fmax;
	state_t *new_state = malloc(sizeof(state_t));

	if (new_state == NULL) {
		EPRINTF("Unable to allocate memory for state\n");
		return NULL;
	}

	fbr = fopen(BRIGHTNESS_CURRENT, "r");
	if (fbr == NULL) {
		EPRINTF("Unable to read current brightness from %s\n",
			BRIGHTNESS_CURRENT);
		goto error;
	}

	fscanf(fbr, "%d", &new_state->current_brightness);
	fclose(fbr);

	fmax = fopen(BRIGHTNESS_MAX, "r");
	if (fmax == NULL) {
		EPRINTF("Unable to read max brightness from %s\n",
			BRIGHTNESS_MAX);
		goto error;
	}

	fscanf(fmax, "%d", &new_state->max_brightness);
	fclose(fmax);

	new_state->bar = bar_init(b_mask, b_attr);

	if (new_state->bar == NULL) {
		EPRINTF("Failed to allocate memory for bar\n");
		goto error;
	}

	return new_state;

error:
	free(new_state);
	return NULL;
}

int main(int argc, char **argv)
{
	unsigned int error, b_mask = 0;
	bar_attr_t b_attr;
	state_t *state;

	if (!parse_args(argc, argv, &b_attr, &b_mask)) {
		return 1;
	}

	state = state_init(b_mask, b_attr);

	if (state == NULL) {
		return 1;
	}

	state->running = grab_keyboard(state);
	state->error = 0;

	run(state);

	error = state->error;
	cleanup(state);

	return error;
}
