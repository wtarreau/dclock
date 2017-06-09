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
#include <sys/param.h>
#include <sys/time.h>
#include <getopt.h>

#include <X11/Xlib.h>

/* default options */
unsigned int width=31, height=22;
int X=-31, Y=-22;
char *bg = "black", *fg = "white", *dc = "white";
char *bp = NULL, *bc[2] = { "red", "yellow" };
int daemonize = 0;
int bcindex = 0;

/* command-line options */
static struct option long_opts[] = {
	{"help",      no_argument, 0, 'h'},
	{"daemon",    no_argument, 0, 'd'},
	{"geometry",  required_argument, 0, 'g'},
	{"bg",        required_argument, 0, 'b'},
	{"fg",        required_argument, 0, 'f'},
	{"dc",        required_argument, 0, 'c'},
	{"bp",        required_argument, 0, 256},
	{"bc",        required_argument, 0, 257},
	{0, 0, 0, 0}
};
const char *optstring = "hdg:b:f:";


void usage(void)
{
	fprintf(stderr, "Usage: dclock [options]\n"
		"\t-[h]elp                   display this help\n"
		"\t-[d]aemon                 daemonize\n"
		"\t-[g]eometry WxH[+X+Y]     set window geometry\n"
		"\t-[b]g <color name>        background color\n"
		"\t-[f]g <color name>        foreground 1 color (digits)\n"
		"\t-d[c] <color name>        foreground 2 color (dots)\n"
		"\t-bp <battery path>        /sys entry to battery to monitor\n"
		"\t-bc <color name>          battery low color (may be repeated)\n"
		);
}


/* globals, yuck */
Display *dpy = NULL;
Window win;
GC gc;
XColor background, foreground, dotcolor, batcolor[2];
int batflash = -1;

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
	int x, i;

	curr = time(NULL);
	tm = localtime(&curr);

	/* clear the window */
	if (batflash < 0)
		XSetForeground(dpy, gc, background.pixel);
	else
		XSetForeground(dpy, gc, batcolor[batflash].pixel);

	XFillRectangle(dpy, win, gc, 0, 0, width, height);

	/* and draw the digits */
	XSetForeground(dpy, gc, foreground.pixel);
	draw_digit(dpy, win, gc,  1,  1, (tm->tm_hour / 10) + '0');
	draw_digit(dpy, win, gc,  8,  1, (tm->tm_hour % 10) + '0');
	draw_digit(dpy, win, gc, 18,  1, (tm->tm_min / 10) + '0');
	draw_digit(dpy, win, gc, 25,  1, (tm->tm_min % 10) + '0');

	draw_digit(dpy, win, gc,  1, 13, (tm->tm_mday / 10) + '0');
	draw_digit(dpy, win, gc,  8, 13, (tm->tm_mday % 10) + '0');
	draw_digit(dpy, win, gc, 14, 13, '/');
	draw_digit(dpy, win, gc, 18, 13, ((tm->tm_mon + 1) / 10) + '0');
	draw_digit(dpy, win, gc, 25, 13, ((tm->tm_mon + 1) % 10) + '0');

	/* draw a 4-pixel horizontal line between the time and date to
	 * represent the day of week. First bar on the left is monday,
	 * last one on the right is sunday. We also draw the first pixel
	 * of every day to make them easier to count.
	 */
	x = 1 + ((tm->tm_wday + 6) % 7) * 4;
	for (i = 0; i < 4; i++)
		XDrawPoint(display, win, gc, x + i, 11);
	
	XSetForeground(dpy, gc, dotcolor.pixel);

	/* draw the blinking colon between hours and seconds */
	if (tm->tm_sec & 1)
		draw_digit(dpy, win, gc, 15,  1, ':');

	/* and the dots delimiting the days */
	for (i = 0; i < 8; i++)
		XDrawPoint(display, win, gc, 1 + i * 4, 11);
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

/* returns 0 if unknown/plugged, 1 for good, 2 for low (less than 10 minutes),
 * 3 for very low (less than 3 minutes)
 */
int battery_status()
{
	char fp[MAXPATHLEN];
	FILE *f;
	int pn, en;

	if (!bp)
		return 0;

	if (snprintf(fp, sizeof(fp), "%s/power_now", bp) > sizeof(fp))
		return 0;

	f = fopen(fp, "r");
	if (!f)
		return 0;

	if (fscanf(f, "%d", &pn) != 1)
		return 0;

	fclose(f);

	if (snprintf(fp, sizeof(fp), "%s/energy_now", bp) > sizeof(fp))
		return 0;

	f = fopen(fp, "r");
	if (!f)
		return 0;

	if (fscanf(f, "%d", &en) != 1)
		return 0;

	fclose(f);

	/* return power low when less than 10 minutes of energy are remaining */
	if (en >= (pn * 10 / 60))
		return 1;
	else if (en >= (pn * 3 / 60))
		return 2;
	else
		return 3;
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
		case '?':
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

		case 'c':
			dc = strdup(optarg);
			break;

		case 'd':
			daemonize = 1;
			break;

		case 256: /* -bp */
			bp = strdup(optarg);
			break;

		case 257: /* -bc */
			bc[bcindex++] = strdup(optarg);
			if (bcindex > 1)
				bcindex = 0;
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
	alloc_color(dc, &dotcolor);
	alloc_color(bc[0], &batcolor[0]);
	alloc_color(bc[1], &batcolor[1]);

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

	if (daemonize) {
		if (fork() != 0)
			exit(0);
		close(0);
		close(1);
		close(2);
		setsid();
	}

	while (1) {
		if (XPending(dpy)) {
			/* flush events */
			XEvent ev;
			XNextEvent(dpy, &ev);
			continue;
		}
		draw(dpy, win);
		XFlush(dpy);

		switch (battery_status()) {
		case 2: // less than 10 minutes
			batflash = !batflash;
			usleep(500000);
			break;
		case 3: // less than 3 minutes
			batflash = !batflash;
			usleep(250000);
			break;
		default: // good or unknown
			batflash = -1;
			usleep(1000000);
			break;
		}
	}
}
