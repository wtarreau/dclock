/*
 * dclock: minimalist digital clock - 2013 - Willy Tarreau
 * Part of the code was borrowed from Jeremy Weatherford's orbitclock because
 * it is quite readable and serves as a documentation on the X API :-)
*/

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/time.h>
#include <getopt.h>

#include <X11/Xlib.h>

/* default options */
unsigned int width=31, height=22;
int X=200, Y=200;
char *bg = "black", *fg = "white";

/* command-line options */
static struct option long_opts[] = {
	{"help",      no_argument, 0, 'h'},
	{"geometry",  required_argument, 0, 'g'},
	{"bg",        required_argument, 0, 'b'},
	{"fg",        required_argument, 0, 'f'},
	{0, 0, 0, 0}
};
const char *optstring = "hg:b:f:";


void usage(void)
{
	fprintf(stderr, "Usage: dclock [options]\n"
		"\t-geometry WxH[+X+Y]     set window geometry\n"
		"\t-bg <color name>        background color\n"
		"\t-fg <color name>        foreground 1 color (tick)\n"
		);
}


/* globals, yuck */
Display *dpy = NULL;
Window win;
GC gc;
XColor background, foreground;

/* draw a digit 'd' with top left corner at (x0,y0). <d> may be one
 * ascii char '0'..'9', ':', '/'. Digits are 9 pixels high and 5 wide. 2 pixels
 * are required between digits. ':' is a single pixel wide, '/' is 3.
 */
void draw_digit(Display *display, Window win, GC gc, int x0, int y0, int d)
{
	/* segments are 3-digit long and numbered this way :
	 *     0
	 *  5     1
	 *     6
	 *  4     2
	 *     3
	 */
	const char seg[10] = { 0x3F, 0x06, 0x5b, 0x4F, 0x66, 0x6d, 0x7d, 0x07, 0x7F, 0x6F };

	if (d == ':') {
		XDrawPoint(display, win, gc, x0, y0 + 3);
		XDrawPoint(display, win, gc, x0, y0 + 5);
		return;
	}

	if (d == '/') {
		XDrawPoint(display, win, gc, x0 + 2, y0 + 1);
		XDrawPoint(display, win, gc, x0 + 2, y0 + 2);

		XDrawPoint(display, win, gc, x0 + 1, y0 + 3);
		XDrawPoint(display, win, gc, x0 + 1, y0 + 4);
		XDrawPoint(display, win, gc, x0 + 1, y0 + 5);

		XDrawPoint(display, win, gc, x0 + 0, y0 + 6);
		XDrawPoint(display, win, gc, x0 + 0, y0 + 7);
		return;
	}

	if (d >= '0' && d <= '9') {
		d = seg[d - '0'];
		if (d & 1) {
			XDrawPoint(display, win, gc, x0 + 1, y0 + 0);
			XDrawPoint(display, win, gc, x0 + 2, y0 + 0);
			XDrawPoint(display, win, gc, x0 + 3, y0 + 0);
		}
		if (d & 2) {
			XDrawPoint(display, win, gc, x0 + 4, y0 + 1);
			XDrawPoint(display, win, gc, x0 + 4, y0 + 2);
			XDrawPoint(display, win, gc, x0 + 4, y0 + 3);
		}
		if (d & 4) {
			XDrawPoint(display, win, gc, x0 + 4, y0 + 5);
			XDrawPoint(display, win, gc, x0 + 4, y0 + 6);
			XDrawPoint(display, win, gc, x0 + 4, y0 + 7);
		}
		if (d & 8) {
			XDrawPoint(display, win, gc, x0 + 1, y0 + 8);
			XDrawPoint(display, win, gc, x0 + 2, y0 + 8);
			XDrawPoint(display, win, gc, x0 + 3, y0 + 8);
		}
		if (d & 16) {
			XDrawPoint(display, win, gc, x0 + 0, y0 + 5);
			XDrawPoint(display, win, gc, x0 + 0, y0 + 6);
			XDrawPoint(display, win, gc, x0 + 0, y0 + 7);
		}
		if (d & 32) {
			XDrawPoint(display, win, gc, x0 + 0, y0 + 1);
			XDrawPoint(display, win, gc, x0 + 0, y0 + 2);
			XDrawPoint(display, win, gc, x0 + 0, y0 + 3);
		}
		if (d & 64) {
			XDrawPoint(display, win, gc, x0 + 1, y0 + 4);
			XDrawPoint(display, win, gc, x0 + 2, y0 + 4);
			XDrawPoint(display, win, gc, x0 + 3, y0 + 4);
		}
		return;
	}
}

void draw(Display *display, Window win)
{
	struct tm *tm;
	time_t curr;

	curr = time(NULL);
	tm = localtime(&curr);

	/* clear the window */
	XSetForeground(dpy, gc, background.pixel);
	XFillRectangle(dpy, win, gc, 0, 0, width, height);

	/* and draw the digits */
	XSetForeground(dpy, gc, foreground.pixel);
	draw_digit(dpy, win, gc,  1,  1, (tm->tm_hour / 10) + '0');
	draw_digit(dpy, win, gc,  8,  1, (tm->tm_hour % 10) + '0');
	if (tm->tm_sec & 1)
		draw_digit(dpy, win, gc, 15,  1, ':');
	draw_digit(dpy, win, gc, 18,  1, (tm->tm_min / 10) + '0');
	draw_digit(dpy, win, gc, 25,  1, (tm->tm_min % 10) + '0');

	draw_digit(dpy, win, gc,  1, 12, (tm->tm_mday / 10) + '0');
	draw_digit(dpy, win, gc,  8, 12, (tm->tm_mday % 10) + '0');
	draw_digit(dpy, win, gc, 14, 12, '/');
	draw_digit(dpy, win, gc, 18, 12, ((tm->tm_mon + 1) / 10) + '0');
	draw_digit(dpy, win, gc, 25, 12, ((tm->tm_mon + 1) % 10) + '0');
}

int errhandler(Display *dpy, XErrorEvent *ev)
{
	char buf[512];

	XGetErrorText(dpy, ev->error_code, buf, sizeof(buf));
	printf("XError %d, %s\n", ev->error_code, buf);
	exit(1);
	return 1;
}

void alloc_color(char *color, XColor *ret)
{
	XColor dummy;
	Colormap cmap = DefaultColormap(dpy, DefaultScreen(dpy));

	if (!XAllocNamedColor(dpy, cmap, color, ret, &dummy)) {
		fprintf(stderr, "Failed to allocate color '%s'\n", color);
		exit(1);
	}
}

int main(int argc, char *argv[])
{
	Window root;
	XGCValues gc_setup;
	XSetWindowAttributes swa;
	int c, index;

	while ((c = getopt_long_only(argc, argv, optstring, long_opts, &index)) != -1) {
		switch (c) {
		case 'h':
			usage();
			exit(0);
			break;

		case 'g':
			XParseGeometry(optarg, &X, &Y, &width, &height);
			break;

		case 'b':
			bg = strdup(optarg);
			break;

		case 'f':
			fg = strdup(optarg);
			break;

		case '?':
			usage();
			exit(0);
			break;
		}
	}

	dpy = XOpenDisplay(NULL);
	if (!dpy) {
		fprintf(stderr, "Failed to open default display.\n");
		exit(1);
	}

	XSetErrorHandler(errhandler);

	alloc_color(bg, &background);
	alloc_color(fg, &foreground);

	if (X < 0)
		X += DisplayWidth(dpy, DefaultScreen(dpy));

	if (Y < 0)
		Y += DisplayHeight(dpy, DefaultScreen(dpy));

	swa.override_redirect = 1; /* unmanaged window */

	root = DefaultRootWindow(dpy);
	win = XCreateWindow(dpy, root, X, Y, width, height, 0,
  			    CopyFromParent, InputOutput, CopyFromParent,
  			    CWOverrideRedirect, &swa);

	XStoreName(dpy, win, "dclock");

	XMapWindow(dpy, win);

	gc = XCreateGC(dpy, win, 0, &gc_setup);

	while (1) {
		if (XPending(dpy)) {
			/* flush events */
			XEvent ev;
			XNextEvent(dpy, &ev);
			continue;
		}
		draw(dpy, win);
		XFlush(dpy);
		sleep(1);
	}
}
