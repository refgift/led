#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>

int string_to_color(const char* str) {
    if (strcmp(str, "BLACK") == 0) return COLOR_BLACK;
    if (strcmp(str, "RED") == 0) return COLOR_RED;
    if (strcmp(str, "GREEN") == 0) return COLOR_GREEN;
    if (strcmp(str, "YELLOW") == 0) return COLOR_YELLOW;
    if (strcmp(str, "BLUE") == 0) return COLOR_BLUE;
    if (strcmp(str, "MAGENTA") == 0) return COLOR_MAGENTA;
    if (strcmp(str, "CYAN") == 0) return COLOR_CYAN;
    if (strcmp(str, "WHITE") == 0) return COLOR_WHITE;
    return COLOR_WHITE; // default
}

static void trim_whitespace(char* str) {
    char* end;
    while (isspace((unsigned char)*str)) str++;
    if (*str == 0) return;
    end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) end--;
    end[1] = '\0';
}

static void set_default_config(ColorConfig* config) {
    config->normal_fg = COLOR_WHITE;
    config->normal_bg = COLOR_BLACK;
    config->selection_fg = COLOR_CYAN;
    config->selection_bg = COLOR_BLACK;
    config->semicolon_fg = COLOR_RED;
    config->semicolon_bg = COLOR_BLACK;
    config->meta_level1_fg = COLOR_BLUE;
    config->meta_level1_bg = COLOR_BLACK;
    config->meta_level2_fg = COLOR_CYAN;
    config->meta_level2_bg = COLOR_BLACK;
    config->meta_level3_fg = COLOR_GREEN;
    config->meta_level3_bg = COLOR_BLACK;
    config->meta_level4_fg = COLOR_YELLOW;
    config->meta_level4_bg = COLOR_BLACK;
    config->reserved_words_fg = COLOR_RED;
    config->reserved_words_bg = COLOR_BLACK;
    strcpy(config->syntax_extensions, ".c,.h,.cpp");
    strcpy(config->reserved_words, "int,char,return,if,else,for,while,do,switch,case,default,break,continue,goto,sizeof,typedef,struct,union,enum,static,extern,auto,register,volatile,const,signed,unsigned,short,long,double,float,void");
}

int load_color_config(ColorConfig* config) {
    set_default_config(config);

    const char* home = getenv("HOME");
    if (!home) return 0;

    char path[512];
    snprintf(path, sizeof(path), "%s/.config/led/colorization.conf", home);

    FILE* file = fopen(path, "r");
    if (!file) return 0; // use defaults

    char line[256];
    while (fgets(line, sizeof(line), file)) {
        // Remove newline
        line[strcspn(line, "\n")] = 0;
        // Skip comments and empty
        if (line[0] == '#' || line[0] == '\0') continue;
        char* eq = strchr(line, '=');
        if (!eq) continue;
        *eq = '\0';
        char* key = line;
        char* value = eq + 1;
        trim_whitespace(key);
        trim_whitespace(value);

        if (strcmp(key, "normal_fg") == 0) config->normal_fg = string_to_color(value);
        else if (strcmp(key, "normal_bg") == 0) config->normal_bg = string_to_color(value);
        else if (strcmp(key, "selection_fg") == 0) config->selection_fg = string_to_color(value);
        else if (strcmp(key, "selection_bg") == 0) config->selection_bg = string_to_color(value);
        else if (strcmp(key, "semicolon_fg") == 0) config->semicolon_fg = string_to_color(value);
        else if (strcmp(key, "semicolon_bg") == 0) config->semicolon_bg = string_to_color(value);
        else if (strcmp(key, "meta_level1_fg") == 0) config->meta_level1_fg = string_to_color(value);
        else if (strcmp(key, "meta_level1_bg") == 0) config->meta_level1_bg = string_to_color(value);
        else if (strcmp(key, "meta_level2_fg") == 0) config->meta_level2_fg = string_to_color(value);
        else if (strcmp(key, "meta_level2_bg") == 0) config->meta_level2_bg = string_to_color(value);
        else if (strcmp(key, "meta_level3_fg") == 0) config->meta_level3_fg = string_to_color(value);
        else if (strcmp(key, "meta_level3_bg") == 0) config->meta_level3_bg = string_to_color(value);
        else if (strcmp(key, "meta_level4_fg") == 0) config->meta_level4_fg = string_to_color(value);
        else if (strcmp(key, "meta_level4_bg") == 0) config->meta_level4_bg = string_to_color(value);
        else if (strcmp(key, "syntax_extensions") == 0) strncpy(config->syntax_extensions, value, sizeof(config->syntax_extensions) - 1);
        else if (strcmp(key, "reserved_words_fg") == 0) config->reserved_words_fg = string_to_color(value);
        else if (strcmp(key, "reserved_words_bg") == 0) config->reserved_words_bg = string_to_color(value);
        else if (strcmp(key, "reserved_words") == 0) strncpy(config->reserved_words, value, sizeof(config->reserved_words) - 1);
    }

    fclose(file);
    return 1;
}