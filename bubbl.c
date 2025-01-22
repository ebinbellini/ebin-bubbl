#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <math.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
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
unsigned frame_number = 0;

void close_x(void) {
    if (icon != NULL)
        imlib_free_image();

    Display *dpy = drw->dpy;
    Window win = drw->root;
    drw_free(drw);
    XDestroyWindow(dpy, win);
    // This XCloseDisplay sometimes hangs...
    XCloseDisplay(dpy);
}

void cleanup(void) {
    close_x();
    remove(fifo_path);
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

void load_xresources(void) {
	XrmInitialize();
    char *resource_manager = XResourceManagerString(drw->dpy);
	if (!resource_manager)
		return;
	XrmDatabase db = XrmGetStringDatabase(resource_manager);

	ColorPreference *c;
	for (c = c_prefs; c < c_prefs + LENGTH(c_prefs); c++) {
		resource_load(db, c->name, (void **)c->dst);
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

void load_icon(char *command) {
    icon = NULL;
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
    
    int file_offset = 0;
    int file_size = 0;
    char file_name[256];
    if (icon_type == IconTypeStatus) {
        if (res_value == 0) {
            file_offset = ((StatusIcon*)icon)->disabled_offset;
            file_size = ((StatusIcon*)icon)->disabled_size;
            snprintf(file_name, sizeof(file_name)-1, "%s-disabled.png", command+2);
        } else {
            file_offset = ((StatusIcon*)icon)->enabled_offset;
            file_size = ((StatusIcon*)icon)->enabled_size;
            res_value = 100;
            snprintf(file_name, sizeof(file_name)-1, "%s-enabled.png", command+2);
        }
    } else {
        file_offset = ((RangeIcon*)icon)->icon_offset;
        file_size = ((RangeIcon*)icon)->icon_size;
        snprintf(file_name, sizeof(file_name)-1, "%s.png", command+2);
    }

    Imlib_Image img = imlib_load_image_mem(file_name, icon_data+file_offset, file_size);

    if (img == NULL) {
        fprintf(stderr, "Couldn't load icon");
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

        // Draw the new color through the mask
        XSetClipMask(drw->dpy, drw->gc, mask);
        XSetForeground(drw->dpy, drw->gc, drw->scheme[ColBg].pixel);
        XFillRectangle(drw->dpy, colored_icon, drw->gc, 0, 0, width, height);
        XSetClipMask(drw->dpy, drw->gc, None);

        // Set new colored image as imlib image
        imlib_context_set_drawable(colored_icon);
        Imlib_Image colored_image = imlib_create_image_from_drawable(mask, 0, 0, width, height, 0);
        imlib_context_set_image(colored_image);

        // Can't free colored_image since it will be used when drawing
        // Cleanup
        XFreePixmap(drw->dpy, colored_icon);
        imlib_context_set_drawable(drw->drawable);
        imlib_free_pixmap_and_mask(pix);
    }
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

void check_new_command(void) {
    int n;
    int fd;
    char buf[256];
    do {
        // While we receive EAGAIN, keep reading
        fd = open(fifo_path, O_RDONLY | O_NONBLOCK);
        n = read(fd, buf, sizeof(buf));
    } while (n == -1 && errno == EAGAIN);

    if (n > 0) {
        // Ok we got a message, but there might be more. Skip to the last
        // one.
        int m = n;
        while (n > 0) {
            n = read(fd, buf, sizeof(buf));
            if (n > 0) {
                m = n;
            }
        }

        // Split e.g. "--volume 80" into command="--volume" and val="80"
        char *val;
        char *command = val = buf;
        while (*val != ' ' && val - command < m) {
            val++;
        }
        *val = '\0';
        val++;

        if (strlen(command) > 2) {
            // Valid command, update data
            frame_number = 0;
            res_value = atoi(val);
            if (icon != NULL) {
                if (icon_type == IconTypeRange) {
                    if (strcmp(command+2, ((RangeIcon *)icon)->command) != 0) {
                        // The command is different, load the new icon
                        load_icon(command);
                    }
                } else {
                    // It's a status icon, so it has probably changed
                    load_icon(command);
                }
            }
        }
    }

    close(fd);
}

void draw(void) {
    // Count how many frames have been drawn
    frame_number++;

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

    check_new_command();

    if (frame_number == 100) {
        cleanup();
        exit(0);
    }
}

void xinit(void) {
    Display *dpy;
    if (!(dpy = XOpenDisplay(NULL))) {
        remove(fifo_path);
        die("unable to open display");
    }

    int screen = DefaultScreen(dpy);

    Window root = DefaultRootWindow(dpy);

    unsigned long black, white;
    black = BlackPixel(dpy, screen);
    white = WhitePixel(dpy, screen);

    unsigned width, height;
    width = DisplayWidth(dpy, screen);
    height = DisplayHeight(dpy, screen);

    int x, y;
    if (!strcmp(initial_x_direction, "left"))
        x = x_offset;
    else
        x = width - BUBBL_SIZE - x_offset;
    if (!strcmp(initial_y_direction, "bottom"))
        y = height - BUBBL_SIZE - y_offset;
    else
        y = y_offset;

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

    if (use_xresources)
        load_xresources();
    char * palette[] = { col_background, col_foreground };
    drw->scheme = drw_scm_create(drw, (const char **)palette, 2);

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

void loop(void) {
    XEvent event;
    KeySym key;
    char text[255];

    while (1) {
        while (XPending(drw->dpy)) {
            XNextEvent(drw->dpy, &event);

            if (event.type == KeyPress) {
                XLookupString(&event.xkey, text, 255, &key, 0);
            }
            // Exit program on mouse button press
            else if (event.type == ButtonPress) {
                // Do not react to mouse scrolling
                if (event.xbutton.button != Button4 && event.xbutton.button != Button5) {
                    cleanup();
                    exit(0);
                }
            }
        }

        draw();
        usleep(timeout * 10000);
    }
}

void interrupt_handler(int _) {
    cleanup();
    exit(0);
}

void check_existing_instance(char *command) {
    int ret = mkfifo(fifo_path, 0666);
    if (ret == -1 && errno == EEXIST) {
        // Another process already exists
        int fd = open(fifo_path, O_WRONLY);
        char buf[256];
        snprintf(buf, sizeof(buf), "%s %d", command, res_value);
        ssize_t n = write(fd, buf, strlen(buf)+1);
        close(fd);
        puts("Sent instructions to existing instance of ebin-bubbl");
        printf("If no such instance is running, please delete the fifo file at"
            " %s\n", fifo_path);
        exit(0);
    }
}

void usage(void) {
	die("Wrong command format. Check out the README.md for usage examples.");
}

int main(int argc, char *argv[]) {
    if (argc > 2 && strncmp("--", argv[1], 2) == 0) {
        res_value = atoi(argv[2]);
    } else {
        usage();
    }

    check_existing_instance(argv[1]);

    xinit();
    load_icon(argv[1]);
    signal(SIGINT, interrupt_handler);
    signal(SIGKILL, interrupt_handler);
    loop();
    return 0;
}

