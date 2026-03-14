#ifndef CONFIG_H
#define CONFIG_H
#include <ncurses.h>
#include <time.h>
#define VERSION "1.0-beta2"
typedef struct {
    int normal_fg, normal_bg;
    int selection_fg, selection_bg;
    int semicolon_fg, semicolon_bg;
    int meta_level1_fg, meta_level1_bg;
    int meta_level2_fg, meta_level2_bg;
    int meta_level3_fg, meta_level3_bg;
    int meta_level4_fg, meta_level4_bg;
    int reserved_words_fg, reserved_words_bg;
} ColorScheme;
typedef struct {
    char extensions[256];
    char reserved_words[1024];
    char paired_keywords[1024];
} SyntaxConfig;
typedef struct {
    int timeout;  // seconds
    int keystrokes;  // keystrokes before auto-save
} AutoSaveConfig;
typedef struct {
    int enabled;
    int show_version;
    int show_time;
    int time_format;  // 12 or 24
    int style;  // 0=balanced, 1=centered, 2=compact
} StatusBarConfig;
typedef struct {
    int show_line_numbers;
    int syntax_highlight;
    int word_wrap;
    int tab_width;
    int spaces_for_tab;
} DisplayConfig;
typedef struct {
    int max_file_size_mb;
    int memory_limit_mb;
} PerformanceConfig;
typedef enum {
    CONFIG_SUCCESS = 0,
    CONFIG_PARSE_ERROR,
    CONFIG_VALIDATION_ERROR,
    CONFIG_FILE_NOT_FOUND,
    CONFIG_PERMISSION_ERROR
} ConfigError;
typedef struct {
    int version;  // Config file format version
    time_t last_modified;  // For hot-reload detection
    
    // Modular configuration sections
    ColorScheme colors;
    SyntaxConfig syntax;
    AutoSaveConfig autosave;
    StatusBarConfig statusbar;
    DisplayConfig display;
    PerformanceConfig performance;
    
    // Validation state
    ConfigError last_error;
} EditorConfig;
ConfigError load_editor_config(EditorConfig* config);
int string_to_color(const char* str);
#endif