#ifndef CONFIG_H
#define CONFIG_H

/**
 * config.h - Configuration system for the led text editor.
 *
 * This header defines the complete runtime configuration model.
 * All user preferences (colors, syntax rules, autosave behavior,
 * display options, etc.) live here.
 *
 * Philosophy: Headers are "hot" — this file must be extremely clear,
 * well-documented, and low-entropy because it is the contract between
 * the editor core and user customization.
 */

#include <ncurses.h>
#include <time.h>

#define VERSION "1.0.6"
/**
 * ColorScheme - ncurses color pairs for different syntactic elements.
 * All values are ncurses color constants (COLOR_BLACK, COLOR_RED, etc.).
 */
typedef struct {
    int normal_fg, normal_bg;
    int selection_fg, selection_bg;
    int semicolon_fg, semicolon_bg;
    int meta_level1_fg, meta_level1_bg;   /* outermost nesting level */
    int meta_level2_fg, meta_level2_bg;
    int meta_level3_fg, meta_level3_bg;
    int meta_level4_fg, meta_level4_bg;   /* deepest nesting and beyond */
    int reserved_words_fg, reserved_words_bg;
} ColorScheme;
/**
 * SyntaxConfig - Rules controlling syntax highlighting.
 * Strings are comma-separated lists (see colorization.conf for format).
 */
typedef struct {
    char extensions[256];        /* file extensions that enable highlighting */
    char reserved_words[1024];   /* keywords to color as reserved_words_fg */
    char paired_keywords[1024];  /* open-close pairs for nesting colors, e.g. "if-then,(,)" */
} SyntaxConfig;
/**
 * AutoSaveConfig - Triggers for automatic saving + backup creation.
 */
typedef struct {
    int timeout;     /* seconds of inactivity before auto-save */
    int keystrokes;  /* number of edits before auto-save */
} AutoSaveConfig;
/**
 * StatusBarConfig - Controls what appears on the bottom status line.
 */
typedef struct {
    int enabled;
    int show_version;
    int show_time;
    int show_key_meter;   /* microsecond response time meter */
    int time_format;      /* 12 or 24 */
    int style;            /* 0=balanced, 1=centered, 2=compact */
} StatusBarConfig;
/**
 * DisplayConfig - Core visual and editing behavior toggles.
 * These are the most frequently changed options at runtime (F2/F3 keys).
 */
typedef struct {
    int show_line_numbers;
    int syntax_highlight;
    int tab_width;
    int spaces_for_tab;   /* if true, TAB inserts spaces instead of '\t' */
    int word_wrap;        /* F3 toggle: when true, long lines wrap visually */
} DisplayConfig;
typedef struct {
    int max_file_size_mb;
    int memory_limit_mb;
    int max_line_length;   /* max chars per logical line before truncation on load */
} PerformanceConfig;

/** SearchConfig - Limits for regex search/replace to prevent DoS. */
typedef struct {
    int enabled;
    int max_pattern_length;
} SearchConfig;

/** ConfigError - Result codes from configuration loading. */
typedef enum {
    CONFIG_SUCCESS = 0,
    CONFIG_PARSE_ERROR,
    CONFIG_VALIDATION_ERROR,
    CONFIG_FILE_NOT_FOUND,
    CONFIG_PERMISSION_ERROR
} ConfigError;

/**
 * EditorConfig - The root configuration object.
 * Loaded once at startup from ~/.config/led/colorization.conf (if present).
 */
typedef struct {
    int version;            /* config file format version */
    time_t last_modified;   /* used for potential future hot-reload */

    /* Modular sections */
    ColorScheme colors;
    SyntaxConfig syntax;
    AutoSaveConfig autosave;
    StatusBarConfig statusbar;
    DisplayConfig display;
    PerformanceConfig performance;
    SearchConfig search;

    ConfigError last_error;
} EditorConfig;

/* Public API */
ConfigError load_editor_config(EditorConfig* config);
int string_to_color(const char* str);
#endif
