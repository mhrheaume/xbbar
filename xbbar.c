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

struct state {
	int current_brightness;
	int max_brightness;

	int running;
	int error;

	struct bar *bar;
};

static void usage();
static int parse_positive_int(char *str);
static int parse_args(int argc,
	char **argv,
	struct bar_attr *b_attr,
	unsigned int *b_mask);

static void write_brightness(struct state *state);
static void brightness_up(struct state *state);
static void brightness_down(struct state *state);

static void handle_kpress(struct state *state, XKeyEvent *e);
static void handle_event(struct state *state, XEvent ev);
static int grab_keyboard(struct state *state);

static void run(struct state *state);
static void cleanup(struct state *state);

static struct state *state_init(unsigned int b_mask, struct bar_attr *b_attr);

void usage()
{
	EPRINTF(
		"usage: xbbar [-v] [-h] [-p <padding>] [-n <nrect>] [-xs <rect_xsz>]\n"
		"             [-ys <rect_ysz>] [-fg <fg_color>] [-bg <bg_color>]\n");
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

int parse_args(int argc, char **argv, struct bar_attr *b_attr, unsigned int *b_mask)
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
			*b_mask |= BAR_MASK_PADDING;

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
			*b_mask |= BAR_MASK_NRECT;

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
			*b_mask |= BAR_MASK_RECT_XSZ;

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
			*b_mask |= BAR_MASK_RECT_YSZ;

			if (b_attr->rect_ysz < 0) {
				EPRINTF("-ys: invalid argument\n");
				return 0;
			}
		} else if (!strcmp(argv[i], "-fg")) {
			if (!(++i < argc)) {
				EPRINTF("-fg: missing argument\n");
				return 0;
			}

			b_attr->fg = argv[i];
			*b_mask |= BAR_MASK_FG;
		} else if (!strcmp(argv[i], "-bg")) {
			if (!(++i < argc)) {
				EPRINTF("-bg: missing argument\n");
				return 0;
			}

			b_attr->bg = argv[i];
			*b_mask |= BAR_MASK_BG;
		} else {
			usage();
			return 0;
		}
	}

	return 1;
}

void write_brightness(struct state *state)
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

void brightness_up(struct state *state)
{
	state->current_brightness += BRIGHTNESS_STEP;
	if (state->current_brightness > state->max_brightness) {
		state->current_brightness = state->max_brightness;
	}

	write_brightness(state);
}

void brightness_down(struct state *state)
{
	state->current_brightness -= BRIGHTNESS_STEP;
	if (state->current_brightness < 0) {
		state->current_brightness = 0;
	}

	write_brightness(state);
}

void handle_kpress(struct state *state, XKeyEvent *e)
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

void handle_event(struct state *state, XEvent ev)
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

int grab_keyboard(struct state *state)
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

void run(struct state *state)
{
	XEvent ev;

	while (state->running && !XNextEvent(state->bar->dpy, &ev)) {
		handle_event(state, ev);
	}
}

void cleanup(struct state *state)
{
	bar_cleanup(state->bar);
	free(state);
}

struct state *state_init(unsigned int b_mask, struct bar_attr *b_attr)
{
	FILE *fbr, *fmax;
	int status;
	struct state *new_state = malloc(sizeof(struct state));

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

	status = bar_init(b_mask, b_attr, &new_state->bar); 

	if (status != BAR_STATUS_SUCCESS) {
		EPRINTF("Error initializing bar: %s\n", bar_status_tostring(status));
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
	struct bar_attr b_attr;
	struct state *state;

	if (!parse_args(argc, argv, &b_attr, &b_mask)) {
		return 1;
	}

	state = state_init(b_mask, &b_attr);

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
