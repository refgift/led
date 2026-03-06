#ifndef CONFIG_H
#define CONFIG_H

#include <ncurses.h>

typedef struct {
    int normal_fg, normal_bg;
    int selection_fg, selection_bg;
    int semicolon_fg, semicolon_bg;
    int meta_level1_fg, meta_level1_bg;
    int meta_level2_fg, meta_level2_bg;
    int meta_level3_fg, meta_level3_bg;
    int meta_level4_fg, meta_level4_bg;
    int reserved_words_fg, reserved_words_bg;
    char syntax_extensions[256];
    char reserved_words[1024];
} ColorConfig;

int load_color_config(ColorConfig* config);
int string_to_color(const char* str);

#endif