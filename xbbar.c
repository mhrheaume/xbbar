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
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <xpb.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/XF86keysym.h>

#ifdef DEBUG
#define DEBUG_PRINTF(...) printf(__VA_ARGS__)
#else
#define DEBUG_PRINTF(...) do {} while (0)
#endif

#define EPRINTF(...) fprintf(stderr, __VA_ARGS__)

#define MSEC_TO_USEC(ms) ms * 1000

#define BRIGHTNESS_DIR "/sys/class/backlight/acpi_video0"
#define BRIGHTNESS_CURRENT BRIGHTNESS_DIR "/brightness"
#define BRIGHTNESS_MAX BRIGHTNESS_DIR "/max_brightness"

#define BRIGHTNESS_STEP 1

struct state {
	int current_brightness;
	int max_brightness;

	bool running;
	bool error;

	struct xpb *bar;
};

static void usage();
static bool parse_int(char *str, int *out);
static bool parse_args(int argc,
	char **argv,
	unsigned long *mask,
	struct xpb_attr *attr);

static void write_brightness(struct state *state);
static void brightness_up(struct state *state);
static void brightness_down(struct state *state);

static void handle_kpress(struct state *state, XKeyEvent *e);
static void handle_event(struct state *state, XEvent ev);
static bool grab_keyboard(struct state *state);

static void run(struct state *state);
static void cleanup(struct state *state);

static struct state *state_init(unsigned long mask, struct xpb_attr *attr);

void usage()
{
	EPRINTF(
		"usage: xbbar [-v] [-h] [-p <padding>] [-n <nrect>] [-xs <rect_xsz>]\n"
		"             [-ys <rect_ysz>] [-fg <fg_color>] [-bg <bg_color>]\n");
}

bool parse_int(char *str, int *out)
{
	long ret;

	errno = 0;
	ret = strtol(str, NULL, 10);

	if (errno == EINVAL || errno == ERANGE || ret > INT_MAX) {
		return false;
	}

	*out = (int)ret;
	return true;
}

// TODO: This is pretty gross..
bool parse_args(int argc, char **argv, unsigned long *mask, struct xpb_attr *attr)
{
	int i, parsed_int;

	for (i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "-v")) {
			printf("xbbar-"VERSION"\n");
			return false;
		} else if (!strcmp(argv[i], "-p")) {
			if (!(++i < argc)) {
				EPRINTF("-p: missing argument\n");
				return false;
			}

			if (!parse_int(argv[i], &parsed_int)) {
				EPRINTF("-p: invalid argument\n");
				return 0;
			}

			attr->padding = parsed_int;
			*mask |= XPB_MASK_PADDING;
		} else if (!strcmp(argv[i], "-n")) {
			if (!(++i < argc)) {
				EPRINTF("-n: missing argument\n");
				return false;
			}

			if (!parse_int(argv[i], &parsed_int)) {
				EPRINTF("-n: invalid argument\n");
				return false;
			}

			attr->nrect = parsed_int;
			*mask |= XPB_MASK_NRECT;
		} else if (!strcmp(argv[i], "-xs")) {
			if (!(++i < argc)) {
				EPRINTF("-xs: missing argument\n");
				return false;
			}

			if (!parse_int(argv[i], &parsed_int)) {
				EPRINTF("-xs: invalid argument\n");
				return false;
			}

			attr->rect_xsz = parsed_int;
			*mask |= XPB_MASK_RECT_XSZ;
		} else if (!strcmp(argv[i], "-ys")) {
			if (!(++i < argc)) {
				EPRINTF("-ys: missing argument\n");
				return false;
			}

			if (!parse_int(argv[i], &parsed_int)) {
				EPRINTF("-ys: invalid argument\n");
				return false;
			}

			attr->rect_ysz = parsed_int;
			*mask |= XPB_MASK_RECT_YSZ;
		} else if (!strcmp(argv[i], "-x")) {
			if (!(++i < argc)) {
				EPRINTF("-x: missing argument\n");
				return false;
			}

			if (!parse_int(argv[i], &parsed_int)) {
				EPRINTF("-x: invalid argument\n");
				return false;
			}

			attr->xpos = parsed_int;
			*mask |= XPB_MASK_XPOS;
		} else if (!strcmp(argv[i], "-y")) {
			if (!(++i < argc)) {
				EPRINTF("-y: missing argument\n");
				return false;
			}

			if (parse_int(argv[i], &parsed_int)) {
				EPRINTF("-y: invalid argument\n");
				return false;
			}

			attr->ypos = parsed_int;
			*mask |= XPB_MASK_YPOS;
		} else if (!strcmp(argv[i], "-fg")) {
			if (!(++i < argc)) {
				EPRINTF("-fg: missing argument\n");
				return false;
			}

			attr->fg = argv[i];
			*mask |= XPB_MASK_FG;
		} else if (!strcmp(argv[i], "-bg")) {
			if (!(++i < argc)) {
				EPRINTF("-bg: missing argument\n");
				return false;
			}

			attr->bg = argv[i];
			*mask |= XPB_MASK_BG;
		} else {
			usage();
			return false;
		}
	}

	return true;
}

void write_brightness(struct state *state)
{
	FILE *fbr = fopen(BRIGHTNESS_DIR "/brightness", "w");
	if (fbr == NULL) {
		EPRINTF("Unable to write current brightness to %s.\n", BRIGHTNESS_MAX);

		state->running = false;
		state->error = true;
		return;
	}

	DEBUG_PRINTF("current: %d; max: %d; perc: %d\n",
		state->current_brightness,
		state->max_brightness,
		state->current_brightness * 100 / state->max_brightness);

	fprintf(fbr, "%d", state->current_brightness);
	fclose(fbr);
}

void brightness_up(struct state *state)
{
	if (state->current_brightness + BRIGHTNESS_STEP >= state->max_brightness) {
		state->current_brightness = state->max_brightness;
	} else {
		state->current_brightness += BRIGHTNESS_STEP;
	}

	write_brightness(state);
}

void brightness_down(struct state *state)
{
	if (state->current_brightness <= BRIGHTNESS_STEP) {
		state->current_brightness = 0;
	} else {
		state->current_brightness -= BRIGHTNESS_STEP;
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
		xpb_draw(state->bar, state->current_brightness, state->max_brightness);
		break;
	case XF86XK_MonBrightnessDown:
	case XK_Down:
	case XK_J:
	case XK_j:
		brightness_down(state);
		xpb_draw(state->bar, state->current_brightness, state->max_brightness);
		break;
	case XK_Escape:
		state->running = false;
		break;
	}
}

void handle_event(struct state *state, XEvent ev)
{
	switch (ev.type) {
	case Expose:
		if (ev.xexpose.count == 0) {
			xpb_draw(state->bar,
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

bool grab_keyboard(struct state *state)
{
	int len;

	for (len = 100; len > 0; len--) {
		int result = XGrabKeyboard(state->bar->dpy,
			state->bar->root,
			True,
			GrabModeAsync,
			GrabModeAsync,
			CurrentTime);

		if (result == GrabSuccess) {
			break;
		}

		usleep(MSEC_TO_USEC(1));
	}

	return len > 0;
}

void run(struct state *state)
{
	XEvent ev;
	int count = 0;

	// Exit if 1.5 seconds pass with no activity
	while (state->running && count < 30) {
		while (XPending(state->bar->dpy)) {
			count = 0;
			XNextEvent(state->bar->dpy, &ev);
			handle_event(state, ev);
		}

		usleep(MSEC_TO_USEC(50));
		count++;
	}
}

void cleanup(struct state *state)
{
	xpb_cleanup(state->bar);
	free(state);
}

struct state *state_init(unsigned long mask, struct xpb_attr *attr)
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

	status = xpb_init(mask, attr, &new_state->bar);

	if (!XPB_SUCCESS(status)) {
		EPRINTF("Error initializing bar: %s\n", xpb_status_tostring(status));
		goto error;
	}

	return new_state;

error:
	free(new_state);
	return NULL;
}

int main(int argc, char **argv)
{
	int error;
	unsigned long mask = 0;
	struct xpb_attr attr;
	struct state *state;

	if (!parse_args(argc, argv, &mask, &attr)) {
		return 1;
	}

	state = state_init(mask, &attr);

	if (state == NULL) {
		return 1;
	}

	state->running = grab_keyboard(state);
	state->error = false;

	run(state);

	error = state->error;
	cleanup(state);

	return error;
}
