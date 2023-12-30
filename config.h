
// Width of the bubble
#define BUBBL_SIZE 156

static const char *fontlist[] = { "Atari ST 8x16 System Font:pixelsize=30" };

// =============== COLORS =============== 

typedef struct {
    char* name;
    char** dst;
} ColorPreference;

// Fallback colors for when colors from XResources are not found
static char* col_background = "#000000";
static char* col_foreground = "#ddf00a";

// Which color names to use when fetching colors from XResources
ColorPreference c_prefs[] = {
    { "foreground", &col_background },
    { "color11", &col_foreground },
};

// =============== ICONS =============== 

// TODO embed icons in program instead of having a directory option
static const char *icon_directory = "/home/ebin/ports/ebin/bubbl/icons/";

/* Icon type */
enum { IconTypeStatus, IconTypeRange };

typedef struct {
    char* command;
    char* disabled_path;
    char* disabled_text;
    char* enabled_path;
    char* enabled_text;
} StatusIcon;

StatusIcon status_icons[] = {
    /*Command       Disabled icon    & text   Enabled icon & text */
    { "sound",      "speaker-muted", "MUTED", "speaker",   "ACTIVE" },
    { "mic",        "mic-muted",     "MUTED", "mic",       "ACTIVE" },
    { "power-mode", "battery",       "SAVE!", "battery",   "FAST!"  },
};

typedef struct {
    char* command;
    char* path;
} RangeIcon;

RangeIcon range_icons[] = {
    /*Command       Icon path */
    { "brightness", "soleil" },
    { "volume",     "speaker" },
};