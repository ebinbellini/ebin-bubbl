#include "icon_data.h"

// Width of the bubble
#define BUBBL_SIZE 156

// Bubble initial horizontal direction, may be "right" or "left"
static const char initial_x_direction[] = "right";
// Bubble initial vertical direction, may be "top" or "bottom"
static const char initial_y_direction[] = "top";

// Bubble horizontal offset
static const int x_offset = 12;
// Bubble vertical offset
static const int y_offset = 50;

// Font config
static const char *fontlist[] = { "Atari ST 8x16 System Font:pixelsize=30" };

// File used to send commands between instances of ebin bubbl
static const char fifo_path[] = "/tmp/ebin_bubbl_pipe";

// =============== COLORS =============== 

typedef struct {
    char* name;
    char** dst;
} ColorPreference;

static const int use_xresources = 1;

// Fallback colors for when Xresources colors are not used/found
static char* col_background = "#000000";
static char* col_foreground = "#ddf00a";

// Which color names to use when fetching colors from Xresources
ColorPreference c_prefs[] = {
    { "foreground", &col_background },
    { "color11", &col_foreground },
};

// =============== ICONS =============== 

// Icon type
enum { IconTypeStatus, IconTypeRange };

typedef struct {
    char* command;

    unsigned int disabled_offset;
    unsigned int disabled_size;
    char* disabled_text;

    unsigned int enabled_offset;
    unsigned int enabled_size;
    char* enabled_text;
} StatusIcon;

#define ICON(X) icon_offset_##X, icon_size_##X
StatusIcon status_icons[] = {
    //Command       Disabled icon    & text   Enabled icon & text
    { "sound",      ICON(speaker_muted), "MUTED", ICON(speaker),   "ACTIVE" },
    { "mic",        ICON(mic_muted),     "MUTED", ICON(mic),       "ACTIVE" },
    { "power-mode", ICON(battery),       "SAVE!", ICON(battery),   "FAST!"  },
};

typedef struct {
    char* command;
    unsigned int icon_offset;
    unsigned int icon_size;
} RangeIcon;

RangeIcon range_icons[] = {
    //Command       Icon
    { "brightness", ICON(soleil) },
    { "volume",     ICON(speaker) },
};