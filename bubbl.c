#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <math.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <errno.h>


#include <Imlib2.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xft/Xft.h>
#include <X11/extensions/shape.h>

#include "drw.h"
#include "util.h"

#define LENGTH(X)               (sizeof X / sizeof X[0])
#define MAX(A, B)               ((A) > (B) ? (A) : (B))
#define MIN(A, B)               ((A) < (B) ? (A) : (B))
#define TEXTW(X)                (drw_fontset_getwidth(drw, (X)))

#define BUBBL_SIZE 156


// Config
static const char *fontlist[] = { "Atari ST 8x16 System Font:pixelsize=30" };
static const char col_black[]       = "#000000";
static const char col_gray[]       = "#444444";
static const char col_main[]        = "#ddf00a";
static const char *colors[][3]      = {
	/* fg         bg        border   */
	{  col_black, col_main, col_main },
	{  col_gray,  col_main, col_gray },
};

/* Resource types */
enum { ResNoType, ResBrightness, ResVolume, ResSoundMute, ResMicMute, ResPower };

// BEGIN globals

Drw *drw;
static Fnt *small;

// Resource
int res_type = ResNoType;
int res_value;

// PNG
/*png_structp png_ptr;
png_infop info_ptr;
Pixmap icon;*/

// END globals

void close_x() {
	if (res_type != ResNoType)
		imlib_free_image();

	Display *dpy = drw->dpy;
	Window win = drw->root;
	drw_free(drw);
	//XFreePixmap(dpy, icon);
	XDestroyWindow(dpy, win);
	XCloseDisplay(dpy);

	exit(0);
}

void get_window_size(Display *dpy, Window win, unsigned *width, unsigned *height) {
	Window dummy_win;
	int dummy_x, dummy_y;
	unsigned int dummy_border, dummy_depth;
	XGetGeometry(dpy, win, &dummy_win, &dummy_x, &dummy_y, width,
			height, &dummy_border, &dummy_depth);
}

void draw_hexagon_width(unsigned x, unsigned y, unsigned sl, unsigned width) {
	const double cos30 = cos(M_PI/6);
	const double sin30 = 0.5f;

	XSetLineAttributes(drw->dpy, drw->gc, width, LineSolid, CapButt, JoinMiter);
	XSetForeground(drw->dpy, drw->gc, drw->scheme[ColBg].pixel);

	XDrawLine(drw->dpy, drw->drawable, drw->gc, x, y - sl,                     x + cos30*sl, y - sin30 * sl);
	XDrawLine(drw->dpy, drw->drawable, drw->gc, x + cos30*sl, y - sin30 * sl,  x + cos30*sl, y + sin30 * sl);
	XDrawLine(drw->dpy, drw->drawable, drw->gc, x + cos30*sl, y + sin30 * sl,  x, y + sl);

	XDrawLine(drw->dpy, drw->drawable, drw->gc, x, y - sl,                     x - cos30*sl, y - sin30 * sl);
	XDrawLine(drw->dpy, drw->drawable, drw->gc, x - cos30*sl, y - sin30 * sl,  x - cos30*sl, y + sin30 * sl);
	XDrawLine(drw->dpy, drw->drawable, drw->gc, x - cos30*sl, y + sin30 * sl,  x, y + sl);
}

double lerp(double x1, double x2, double part) {
	// Part is between 0.0 and 1.0
	return x1 * (1-part) + x2 * part;
}

void draw_hexagon_part(unsigned x, unsigned y, unsigned sl, double part) {
	const double cos30 = cos(M_PI/6);
	const double sin30 = 0.5f;

	XSetLineAttributes(drw->dpy, drw->gc, 14, LineSolid, CapButt, JoinMiter);
	XSetForeground(drw->dpy, drw->gc, drw->scheme[ColBg].pixel);

	const double points[6][4] = {
		// start x     start y          end x         end y
		{x,            y - sl,          x + cos30*sl, y - sin30 * sl },
		{x + cos30*sl, y - sin30 * sl,  x + cos30*sl, y + sin30 * sl },
		{x + cos30*sl, y + sin30 * sl,  x,            y + sl         },

		{x,            y + sl,          x - cos30*sl, y + sin30 * sl },
		{x - cos30*sl, y + sin30 * sl,  x - cos30*sl, y - sin30 * sl },
		{x - cos30*sl, y - sin30 * sl,  x,            y - sl,        },
	};

	// The hexagon is divided into 6 lines
	for (int i = 0; i < 6; i++) {
		if (part >= (double)(i+1) / 6.0f) {
			// Draw one whole line
			XDrawLine(drw->dpy, drw->drawable, drw->gc, points[i][0], points[i][1], points[i][2], points[i][3]);
		} else {
			// Draw part of a line
			const double lp = (part * 6 - i);
			double end_x = lerp(points[i][0], points[i][2], lp);
			double end_y = lerp(points[i][1], points[i][3], lp);
			XDrawLine(drw->dpy, drw->drawable, drw->gc, points[i][0], points[i][1], end_x, end_y);
			break;
		}
	}
}

void draw_hexagon_shape(Drawable *dwb, GC *gc, unsigned x, unsigned y, unsigned sl) {
	const double cos30 = cos(M_PI/6);
	const double sin30 = 0.5f;

	const double width = cos30 * sl * 2;
	const double offset = sl - cos30 * sl;

	// Draw left half
	for (int i = 0; i <= width / 2; i++) {
		// Start: sl * 1.5
		// Middle: sl * 2
		const int bottom = ((sl * 1.5 * (width-i*2)) + (sl * 2 * i*2)) / width;
		const int top = (sl * 0.5 * (width-i*2)) / width;
		XDrawLine(drw->dpy, *dwb, *gc, offset+i, bottom, offset+i, top);
	}

	// Draw right half
	for (int i = 0; i <= width / 2; i++) {
		// Start: sl * 1.5
		// Middle: sl * 2
		const int bottom = ((sl * 2 * (width-i*2)) + (sl * 1.5 * i*2)) / width;
		const int top = (sl * 0.5 * (i*2)) / width;
		XDrawLine(drw->dpy, *dwb, *gc, offset+i+width/2, bottom, offset+i+width/2, top);
	}
}

void draw() {
	// Count how many frames have been drawn
	static unsigned f = 0;
	f++;

	/*// Read window size
	unsigned width, height;
	get_window_size(drw->dpy, drw->root, &width, &height);*/

	draw_hexagon_part(BUBBL_SIZE / 2, BUBBL_SIZE/2, BUBBL_SIZE/2, (double)res_value/100);
	draw_hexagon_width(BUBBL_SIZE / 2, BUBBL_SIZE/2, BUBBL_SIZE/2, 1);
	draw_hexagon_width(BUBBL_SIZE / 2, BUBBL_SIZE/2, BUBBL_SIZE/2 - 8, 1);

	// Set text
	drw->fonts = small;
	char text[7] = "";
	if (res_type == ResSoundMute || res_type == ResMicMute || res_type == ResPower) {
		if (res_type == ResSoundMute || res_type == ResMicMute) {
			if (res_value == 0) {
				strcpy(text, "MUTED");
			} else {
				strcpy(text, "ACTIVE");
			}
		} else if (res_type == ResPower) {
			if (res_value == 0) {
				strcpy(text, "SAVE!");
			} else {
				strcpy(text, "FAST!");
			}
		}
	} else {
		snprintf(text, 5, "%d%%", res_value);
	}

	if (res_type == ResNoType)  {
		drw_text(drw, BUBBL_SIZE / 2 - TEXTW(text) / 2, BUBBL_SIZE/2 + 10, TEXTW(text), 10, 0, text, 1);
	} else {
    	int width = imlib_image_get_width();
		drw_text(drw, BUBBL_SIZE / 2 - TEXTW(text) / 2, BUBBL_SIZE/2 + 3, TEXTW(text), 30, 0, text, 1);
		imlib_render_image_on_drawable(BUBBL_SIZE/2 - width / 2, 23);
	}

	// Put frame on screen
	drw_map(drw, drw->root, 0, 0, BUBBL_SIZE, BUBBL_SIZE);

	// Clear pixmap
	drw_rect(drw, 0, 0, BUBBL_SIZE, BUBBL_SIZE, 1, 0);
	
	if (f == 100) close_x();
}

void xinit() {
	Display *dpy = XOpenDisplay(NULL);
	if (dpy == NULL) {
		puts("display är NULL nu");
	}

	int screen = DefaultScreen(dpy);

	Window root = DefaultRootWindow(dpy);

	unsigned long black, white;
	black = BlackPixel(dpy, screen);
	white = WhitePixel(dpy, screen);

	unsigned width, height;
	width = DisplayWidth(dpy, screen);
	height = DisplayHeight(dpy, screen);

	const int x = width - BUBBL_SIZE - 12;
	const int y = 116;

	XSetWindowAttributes swa;
	swa.override_redirect = True;
	swa.event_mask = ExposureMask | KeyPressMask | VisibilityChangeMask;
	Window win = XCreateWindow(dpy, root, x, y, BUBBL_SIZE, BUBBL_SIZE, 0,
			CopyFromParent, CopyFromParent, CopyFromParent,
			CWOverrideRedirect | CWBackPixel | CWEventMask, &swa);

	XSetStandardProperties(dpy, win, "Ebin Bubbl", "Ebin Bubbl", None, NULL, 0, NULL);

	// Init suckless drw library
	drw = drw_create(dpy, screen, win, BUBBL_SIZE, BUBBL_SIZE);

	XSetBackground(drw->dpy, drw->gc, black);
	XSetForeground(drw->dpy, drw->gc, white);

	XSelectInput(dpy, win, ExposureMask|ButtonPressMask|KeyPressMask);
	XClearWindow(drw->dpy, drw->root);
	XMapRaised(drw->dpy, drw->root);

	small = drw_fontset_create(drw, fontlist, LENGTH(fontlist));
	drw->scheme = drw_scm_create(drw, colors[0], 3);

	// Set up X SHAPE Extension
	Pixmap shape_mask = XCreatePixmap(drw->dpy, drw->root,
			BUBBL_SIZE, BUBBL_SIZE, 1);
	GC shape_gc = XCreateGC(drw->dpy, shape_mask, 0, NULL);
	XSetForeground(drw->dpy, shape_gc, 0);
	XFillRectangle(drw->dpy, shape_mask, shape_gc,
			0, 0, BUBBL_SIZE, BUBBL_SIZE);
	XSetForeground (drw->dpy, shape_gc, 1);
	draw_hexagon_shape(&shape_mask, &shape_gc, BUBBL_SIZE / 2, BUBBL_SIZE/2, BUBBL_SIZE/2);
	XShapeCombineMask(drw->dpy, drw->root, ShapeBounding,
			   0, 0, shape_mask, ShapeSet);
	XFreePixmap(drw->dpy, shape_mask);
	XFreeGC(drw->dpy, shape_gc);
}

void load_icon() {
    const char *file_name = NULL;
	char file_path[80] = "/home/ebin/ports/ebin/ebin-bubbl/icons/";
	
	switch(res_type) {
		case ResBrightness:
			file_name = "soleil.png";
			break;
		case ResVolume:
			file_name = "speaker.png";
			break;
		case ResSoundMute:
			if (res_value == 0)
				file_name = "speaker-muted.png";
			else
				file_name = "speaker.png";
			break;
		case ResMicMute:
			if (res_value == 0)
				file_name = "mic-muted.png";
			else
				file_name = "mic.png";
			break;
		case ResPower:
			file_name = "battery.png";
			break;
		default:
			return;
	}
	strncat(file_path, file_name, 80);

	// From https://stackoverflow.com/questions/14995104/how-to-load-bmp-file-using-x11-window-background
    Imlib_Image img = imlib_load_image(file_path);
    if (!img) {
        fprintf(stderr, "%s:Unable to load image\n", file_path);
    }
    imlib_context_set_image(img);

	Screen *scn = DefaultScreenOfDisplay(drw->dpy);
	imlib_context_set_display(drw->dpy);
    imlib_context_set_visual(DefaultVisualOfScreen(scn));
    imlib_context_set_colormap(DefaultColormapOfScreen(scn));
    imlib_context_set_drawable(drw->drawable);
}


void sig_handler(int signo) {
	printf("tog emot signalen %d\n", signo); 
	die("shit asså");
}

void loop() {
	XEvent event;
	KeySym key;
	char text[255];

	while (1) {
		while (XPending(drw->dpy)) {
			XNextEvent(drw->dpy, &event);

			if (event.type == KeyPress) {
				XLookupString(&event.xkey, text, 255, &key, 0);
			}
		}

		draw();
		usleep(10000);
	}
}

void main(int argc, char *argv[]) {
	for (int i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "--brightness")) {
			res_type = ResBrightness;
		} else if (!strcmp(argv[i], "--volume")) {
			res_type = ResVolume;
		} else if (!strcmp(argv[i], "--sound-mute")) {
			res_type = ResSoundMute;
		} else if (!strcmp(argv[i], "--mic-mute")) {
			res_type = ResMicMute;
		} else if (!strcmp(argv[i], "--power-mode")) {
			res_type = ResPower;
		}
		if (++i < argc) {
			res_value = atoi(argv[i]);
		}

		if (res_type == ResSoundMute || res_type == ResMicMute || res_type == ResPower) {
			if (res_value == 1) {
				res_value = 0;
			} else {
				res_value = 100;
			}
		}
	}

	xinit();
	load_icon();

	loop();
	close_x();
}

