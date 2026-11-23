#ifndef CONFIG_H
#define CONFIG_H

/**
 * config.h - Configuration system for the led text editor.
 *
 * This header defines the runtime configuration model.
 * User preferences (colors, syntax rules, autosave behavior,
 * display options, etc.) and their structures and interfaces
 * are here.
 *
 * The editor version number is held here.
 *
 */

#include "frontend/frontend.h"
#include <time.h>

#define VERSION "1.0.8"

#define LED_MAX_SYNTAX_PAIRS 10
/**
 * SyntaxPair - one open/close keyword pair (e.g. "if"/"then", "(" / ")").
 * Pre-parsed form of a "open-close" token from paired_keywords.
 */
typedef struct {
    char open[32];
    char close[32];
} SyntaxPair;
/**
 * ColorScheme - colours for different syntactic elements.
 * All values are LED_COLOR_* from frontend.h.
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
    int comment_fg, comment_bg;
    int string_fg, string_bg;
    int number_fg, number_bg;
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
#define LED_MAX_LANGUAGES 16
/**
 * Language - how one programming language is read and coloured.
 * Chosen by file extension. Built in: c, first, text. More can be
 * defined, and these changed, in colorization.conf with keys of the
 * form NAME.KEY, for example python.extensions=.py
 */
typedef struct {
    char name[32];
    char extensions[256];        /* ".c,.h" */
    char reserved_words[1024];   /* comma-separated */
    char paired_keywords[256];   /* "BEGIN-END,IF-THEN" */
    char line_comment[8];        /* "//"; empty if the language has none */
    char block_open[8];          /* slash-star; empty if none */
    char block_close[8];
    char quotes[8];              /* each character that opens a string */
    int escapes;                 /* a backslash escapes the next character in a string */
    int highlight;               /* 0: shown as plain text */
    int comment_starts_word;     /* the line comment mark counts only at the
                                    start of a word: "#" in shell, not "$#" */
    char indent_after[128];      /* Enter indents after a line ending in one
                                    of these words: "then,do,else" */
    char interpreters[128];      /* a file with no known extension whose first
                                    line is "#!...NAME" is this language */
} Language;

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
    int show_border;      /* F4 toggle: when false, no box() — prevents xterm copy of border */
    int system_clipboard; /* copy and cut also go to the system clipboard (OSC 52) */
    int persistent_undo;  /* keep undo history between sessions (see undo_store.h) */
    int line_numbers_configured; /* 1 if show_line_numbers was set in the config file */
} DisplayConfig;
typedef struct {
    int max_file_size_mb;
    int memory_limit_mb;
    int max_line_length;   /* a longer line is shown without syntax colours */
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
 * Loaded once at startup from colorization.conf, then led.conf, in
 * ~/.config/led (or under --prefix). Either file may hold any key.
 */
typedef struct {
    int version;            /* config file format version */
    time_t last_modified;   /* used for potential future hot-reload */

    /* Modular sections */
    ColorScheme colors;
    SyntaxConfig syntax;    /* the words of the language in use */

    /* Every language led knows, and the one chosen for the open file.
     * config_select_language copies that one's words into `syntax`. */
    Language languages[LED_MAX_LANGUAGES];
    int num_languages;
    int active_language;    /* index into languages, or -1 for none */
    AutoSaveConfig autosave;
    StatusBarConfig statusbar;
    DisplayConfig display;
    PerformanceConfig performance;
    SearchConfig search;

    ConfigError last_error;

    /* Derived pre-parsed syntax data, built by load_editor_config().
     * Reserved words are stored sorted for binary search so the view
     * never re-parses the raw strings per word/per frame. */
    SyntaxPair syntax_pairs[LED_MAX_SYNTAX_PAIRS];
    int num_syntax_pairs;
    char reserved_sorted[1024];  /* words packed NUL-separated */
    int reserved_offset[256];    /* offsets of each word in reserved_sorted */
    int reserved_count;
} EditorConfig;

/* Public API */
ConfigError load_editor_config(EditorConfig* config);
/** Use dir in place of $HOME/.config (the --prefix option): the files
 *  are then dir/led/led.conf and dir/led/colorization.conf. NULL or ""
 *  goes back to $HOME/.config. Call before load_editor_config. */
void config_set_prefix(const char* dir);
/** Create led.conf and colorization.conf, with every default shown as a
 *  comment, where one is missing. An existing file is never touched.
 *  Returns how many files were created. */
int config_write_defaults(void);
/** Fill config with the built-in defaults. Reads no file. Follow with
 *  config_parse_syntax to prepare the keyword tables. */
void set_default_config(EditorConfig* config);
void config_parse_syntax(EditorConfig* config);

/** Choose the language for filename by its extension and make it the
 *  one in use: its words are copied into config->syntax and parsed.
 *  Returns the language, or NULL if no language claims the file (the
 *  words in use are then left as they were). */
const Language* config_select_language(EditorConfig* config, const char* filename);

/** For a file whose extension chose nothing: choose by its first line,
 *  "#!/bin/sh" or "#!/usr/bin/env bash". Returns the language or NULL. */
const Language* config_select_by_first_line(EditorConfig* config, const char* first_line);

/** True if word is in the comma-separated list. */
int config_word_listed(const char* list, const char* word, int word_len);

/** The rules for reading the language in use: comments and strings.
 *  Never NULL; a config with no language chosen reads as C. */
const Language* config_language(const EditorConfig* config);
int config_is_reserved_word(const EditorConfig* config, const char* word);
int string_to_color(const char* str);
#endif
