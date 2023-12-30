#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <math.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <errno.h>

#include <Imlib2.h>
#include <X11/Xlib.h>
#include <X11/Xresource.h>
#include <X11/Xutil.h>
#include <X11/Xft/Xft.h>
#include <X11/extensions/shape.h>

#include "config.h"
#include "drw/drw.h"
#include "drw/util.h"

#define LENGTH(X)               (sizeof X / sizeof X[0])
#define MAX(A, B)               ((A) > (B) ? (A) : (B))
#define MIN(A, B)               ((A) < (B) ? (A) : (B))
#define TEXTW(X)                (drw_fontset_getwidth(drw, (X)))

static Drw *drw;
static Fnt *font;
// The type of icon to show
static int icon_type = IconTypeStatus;
static void* icon = NULL;
// The number for the value
static int res_value;

void close_x() {
    if (icon != NULL)
        imlib_free_image();

    Display *dpy = drw->dpy;
    Window win = drw->root;
    drw_free(drw);
    XDestroyWindow(dpy, win);
    XCloseDisplay(dpy);

    exit(0);
}

int resource_load(XrmDatabase db, char *name, void **dst) {
	char *type;
	XrmValue value;

	char fullname[256];
	char fullclass[256];
	snprintf(fullname, sizeof(fullname), "%s.%s", "bubbl", name);
	snprintf(fullclass, sizeof(fullclass), "%s.%s", "Bubbl", name);
    // TODO is this necessary?
	fullname[sizeof(fullname) - 1] = fullclass[sizeof(fullclass) - 1] = '\0';

	XrmGetResource(db, fullname, fullclass, &type, &value);
	if (value.addr == NULL || strncmp("String", type, 64))
		return 1;

    *dst = value.addr;
	return 0;
}

int load_colors_from_xresources() {
	XrmInitialize();
    char *resource_manager = XResourceManagerString(drw->dpy);
	if (!resource_manager)
		return;
	XrmDatabase db = XrmGetStringDatabase(resource_manager);

	ColorPreference *c;
	for (c = c_prefs; c < c_prefs + LENGTH(c_prefs); c++) {
		resource_load(db, c->name, c->dst);
    }
}


void get_window_size(Display *dpy, Window win, unsigned *width, unsigned *height) {
    Window dummy_win;
    int dummy_x, dummy_y;
    unsigned int dummy_border, dummy_depth;
    XGetGeometry(dpy, win, &dummy_win, &dummy_x, &dummy_y, width,
            height, &dummy_border, &dummy_depth);
}


int get_window_depth(Display *dpy, Window win) {
    Window dummy_win;
    int dummy_x, dummy_y;
    unsigned int dummy_width, dummy_height, dummy_border, depth;
    XGetGeometry(dpy, win, &dummy_win, &dummy_x, &dummy_y, &dummy_width,
            &dummy_height, &dummy_border, &depth);
    return depth;
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
    drw->fonts = font;
    char text[7] = "";
    if (icon != NULL && icon_type == IconTypeStatus) {
        // It's a status icon, show status text
        if (res_value == 0) {
            strcpy(text, ((StatusIcon*)icon)->disabled_text);
        } else {
            strcpy(text, ((StatusIcon*)icon)->enabled_text);
        }
    } else {
        // It's a range icon, show number value
        snprintf(text, 5, "%d%%", res_value);
    }

    if (icon == NULL)  {
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

    font = drw_fontset_create(drw, fontlist, LENGTH(fontlist));

    load_colors_from_xresources();
    char * palette[] = { col_background, col_foreground };
    drw->scheme = drw_scm_create(drw, palette, 2);

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

void load_icon(char *command) {
    char *file_name = NULL;
    char file_path[256];

    for (RangeIcon *ri = range_icons; ri < range_icons + LENGTH(range_icons); ri++) {
        // Skip the first two letters "--" of command 
        if (strcmp(ri->command, command+2) == 0) {
            icon = ri;
            icon_type = IconTypeRange;
            break;
        }
    }

    if (icon == NULL) {
        for (StatusIcon *si = status_icons; si < status_icons + LENGTH(status_icons); si++) {
            // Skip the first two letters "--" of command 
            if (strcmp(si->command, command+2) == 0) {
                icon = si;
                icon_type = IconTypeStatus;
                break;
            }
        }
    }
    
    if (icon == NULL) return;
    
    if (icon_type == IconTypeStatus) {
        if (res_value == 0) {
            file_name = ((StatusIcon*)icon)->disabled_path;
        } else {
            file_name = ((StatusIcon*)icon)->enabled_path;
            res_value = 100;
        }
    } else {
        file_name = ((RangeIcon*)icon)->path;
    }

    snprintf(file_path, LENGTH(file_path), "%s%s.png", icon_directory, file_name);

    Imlib_Image img = imlib_load_image(file_path);
    if (img == NULL) {
        fprintf(stderr, "Couldn't load icon at %s\n", file_path);
        icon = NULL;
    } else {
        Screen *scn = DefaultScreenOfDisplay(drw->dpy);
        imlib_context_set_display(drw->dpy);
        imlib_context_set_visual(DefaultVisualOfScreen(scn));
        imlib_context_set_colormap(DefaultColormapOfScreen(scn));
        imlib_context_set_image(img);
        imlib_context_set_drawable(drw->drawable);
        int width = imlib_image_get_width();
        int height = imlib_image_get_height();
        int depth = get_window_depth(drw->dpy, drw->root);

        // Change color of icon to match palette. First step: create mask
        Pixmap colored_icon = XCreatePixmap(drw->dpy, drw->drawable,
            width, height, depth);
        Pixmap pix, mask;
        imlib_render_pixmaps_for_whole_image(&pix, &mask);

        Clr *color;
        if (XftColorAllocName(drw->dpy, DefaultVisual(drw->dpy, drw->screen),
	                       DefaultColormap(drw->dpy, drw->screen),
	                       col_foreground, color)) {
            // Draw the new color through the mask
            XSetClipMask(drw->dpy, drw->gc, mask);
            XSetForeground(drw->dpy, drw->gc, color->pixel);
            XFillRectangle(drw->dpy, colored_icon, drw->gc, 0, 0, width, height);
            XSetClipMask(drw->dpy, drw->gc, None);

            // Set new colored image as imlib image
            imlib_context_set_drawable(colored_icon);
            Imlib_Image colored_image = imlib_create_image_from_drawable(mask, 0, 0, width, height, 0);
            imlib_context_set_image(colored_image);
            // Can't free colored_image since it will be used when drawing

            // Cleanup
            XftColorFree(drw->dpy, DefaultVisual(drw->dpy, drw->screen),
                DefaultColormap(drw->dpy, drw->screen), color);
        }

        XFreePixmap(drw->dpy, colored_icon);
        imlib_context_set_drawable(drw->drawable);
        imlib_free_pixmap_and_mask(pix);
    }
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

void usage(void) {
	die("Wrong command format. Check out the README.md for usage examples.");
}

void main(int argc, char *argv[]) {
    char* command;
    if (strncmp("--", argv[1], 2) == 0) {
        command = argv[1];
    } else {
        usage();
    }

    if (argc > 2) {
        res_value = atoi(argv[2]);
    } else {
        usage();
    }

    xinit();
    load_icon(command);

    loop();
    close_x();
}

