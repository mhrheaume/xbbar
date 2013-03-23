#include <stdio.h>
#include <stdlib.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

#define XSIZE 300
#define YSIZE 25

typedef struct state {
	Display *dpy;
	Window root;
	Window win;
} state_t;

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
	y = DisplayHeight(state->dpy, screen) * 7 / 8 - (YSIZE / 2);

	return
	XCreateWindow(state->dpy, state->root, x, y, XSIZE, YSIZE, 0,
		DefaultDepth(state->dpy, screen), CopyFromParent,
		DefaultVisual(state->dpy, screen), vmask, &wa);
}

void draw(state_t *state)
{
	GC green_gc;
	XColor green_col;
	Colormap colormap;
	char *green = "#00FF00";

	colormap = DefaultColormap(state->dpy, 0);
	green_gc = XCreateGC(state->dpy, state->win, 0, 0);

	XParseColor(state->dpy, colormap, green, &green_col);
	XAllocColor(state->dpy, colormap, &green_col);

	XSetForeground(state->dpy, green_gc, green_col.pixel);

	XDrawRectangle(state->dpy, state->win, green_gc, 1, 1, 10, 20);
	XDrawRectangle(state->dpy, state->win, green_gc, 16, 1, 10, 20);
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
	case VisibilityNotify:
		break;
	}
}

void run(state_t *state)
{
	XEvent ev;

	while (!XNextEvent(state->dpy, &ev)) {
		handle_event(state, ev);
	}
}

void cleanup(state_t *state)
{
	free(state);
}

int main(int argc, char **argv)
{
	state_t *state = malloc(sizeof(state_t));
	XSetWindowAttributes wa;

	state->dpy = XOpenDisplay(NULL);
	state->root = RootWindow(state->dpy, 0);
	state->win = createWindow(state);

	XMapRaised(state->dpy, state->win);
	XFlush(state->dpy);

	run(state);

	cleanup(state);
	return 0;
}
