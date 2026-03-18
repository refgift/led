#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>
int
string_to_color (const char *str)
{
  if (strcmp (str, "BLACK") == 0)
    return COLOR_BLACK;
  if (strcmp (str, "RED") == 0)
    return COLOR_RED;
  if (strcmp (str, "GREEN") == 0)
    return COLOR_GREEN;
  if (strcmp (str, "YELLOW") == 0)
    return COLOR_YELLOW;
  if (strcmp (str, "BLUE") == 0)
    return COLOR_BLUE;
  if (strcmp (str, "MAGENTA") == 0)
    return COLOR_MAGENTA;
  if (strcmp (str, "CYAN") == 0)
    return COLOR_CYAN;
  if (strcmp (str, "WHITE") == 0)
    return COLOR_WHITE;
  return COLOR_WHITE;           // default
}
static void
trim_whitespace (char *str)
{
  char *end;
  while (isspace ((unsigned char) *str))
    str++;
  if (*str == 0)
    return;
  end = str + strlen (str) - 1;
  while (end > str && isspace ((unsigned char) *end))
    end--;
  end[1] = '\0';
}
static void
set_default_config (EditorConfig *config)
{
  config->version = 2;
  config->last_modified = time (NULL);
  config->last_error = CONFIG_SUCCESS;
  // Colors
  config->colors.normal_fg = COLOR_WHITE;
  config->colors.normal_bg = COLOR_BLACK;
  config->colors.selection_fg = COLOR_BLACK;
  config->colors.selection_bg = COLOR_WHITE;
  config->colors.semicolon_fg = COLOR_RED;
  config->colors.semicolon_bg = COLOR_BLACK;
  config->colors.meta_level1_fg = COLOR_BLUE;
  config->colors.meta_level1_bg = COLOR_BLACK;
  config->colors.meta_level2_fg = COLOR_CYAN;
  config->colors.meta_level2_bg = COLOR_BLACK;
  config->colors.meta_level3_fg = COLOR_GREEN;
  config->colors.meta_level3_bg = COLOR_BLACK;
  config->colors.meta_level4_fg = COLOR_YELLOW;
  config->colors.meta_level4_bg = COLOR_BLACK;
  config->colors.reserved_words_fg = COLOR_RED;
  config->colors.reserved_words_bg = COLOR_BLACK;
  // Syntax
  strcpy (config->syntax.extensions, ".c,.h,.cpp");
  strcpy (config->syntax.reserved_words,
          "int,char,return,if,else,for,while,do,switch,case,default,break,continue,goto,sizeof,typedef,struct,union,enum,static,extern,auto,register,volatile,const,signed,unsigned,short,long,double,float,void");
  strcpy (config->syntax.paired_keywords, "if-then,begin-end,(,)");
  // Auto-save
  config->autosave.timeout = 8000000;
  config->autosave.keystrokes = 50;
  // Status bar
  config->statusbar.enabled = 1;
  config->statusbar.show_version = 1;
  config->statusbar.show_time = 0;
  config->statusbar.time_format = 24;
  config->statusbar.style = 1;  // balanced
  // Display
  config->display.show_line_numbers = 0;
  config->display.syntax_highlight = 0;
  config->display.word_wrap = 0;
  config->display.tab_width = 8;
  config->display.spaces_for_tab = 1;
  // Performance
  config->performance.max_file_size_mb = 10;
  config->performance.memory_limit_mb = 50;
}
ConfigError
load_editor_config (EditorConfig *config)
{
  set_default_config (config);
  const char *home = getenv ("HOME");
  if (!home)
    return CONFIG_SUCCESS;
  char path[512];
  snprintf (path, sizeof (path), "%s/.config/led/colorization.conf", home);
  FILE *file = fopen (path, "r");
  if (!file)
    return CONFIG_SUCCESS;      // use defaults
  char line[256];
  while (fgets (line, (int) sizeof (line), file))
    {
      // Remove newline
      line[strcspn (line, "\n")] = 0;
      // Skip comments and empty
      if (line[0] == '#' || line[0] == '\0')
        continue;
      char *eq = strchr (line, '=');
      if (!eq)
        continue;
      *eq = '\0';
      char *key = line;
      char *value = eq + 1;
      trim_whitespace (key);
      trim_whitespace (value);
      // Colors
      if (strcmp (key, "normal_fg") == 0)
        config->colors.normal_fg = string_to_color (value);
      else if (strcmp (key, "normal_bg") == 0)
        config->colors.normal_bg = string_to_color (value);
      else if (strcmp (key, "selection_fg") == 0)
        config->colors.selection_fg = string_to_color (value);
      else if (strcmp (key, "selection_bg") == 0)
        config->colors.selection_bg = string_to_color (value);
      else if (strcmp (key, "semicolon_fg") == 0)
        config->colors.semicolon_fg = string_to_color (value);
      else if (strcmp (key, "semicolon_bg") == 0)
        config->colors.semicolon_bg = string_to_color (value);
      else if (strcmp (key, "meta_level1_fg") == 0)
        config->colors.meta_level1_fg = string_to_color (value);
      else if (strcmp (key, "meta_level1_bg") == 0)
        config->colors.meta_level1_bg = string_to_color (value);
      else if (strcmp (key, "meta_level2_fg") == 0)
        config->colors.meta_level2_fg = string_to_color (value);
      else if (strcmp (key, "meta_level2_bg") == 0)
        config->colors.meta_level2_bg = string_to_color (value);
      else if (strcmp (key, "meta_level3_fg") == 0)
        config->colors.meta_level3_fg = string_to_color (value);
      else if (strcmp (key, "meta_level3_bg") == 0)
        config->colors.meta_level3_bg = string_to_color (value);
      else if (strcmp (key, "meta_level4_fg") == 0)
        config->colors.meta_level4_fg = string_to_color (value);
      else if (strcmp (key, "meta_level4_bg") == 0)
        config->colors.meta_level4_bg = string_to_color (value);
      else if (strcmp (key, "reserved_words_fg") == 0)
        config->colors.reserved_words_fg = string_to_color (value);
      else if (strcmp (key, "reserved_words_bg") == 0)
        config->colors.reserved_words_bg = string_to_color (value);
      // Syntax
      else if (strcmp (key, "syntax_extensions") == 0)
        strncpy (config->syntax.extensions, value,
                 sizeof (config->syntax.extensions) - 1);
      else if (strcmp (key, "reserved_words") == 0)
        strncpy (config->syntax.reserved_words, value,
                 sizeof (config->syntax.reserved_words) - 1);
      else if (strcmp (key, "paired_keywords") == 0)
        strncpy (config->syntax.paired_keywords, value,
                 sizeof (config->syntax.paired_keywords) - 1);
      // Auto-save
      else if (strcmp (key, "auto_save_timeout") == 0)
        config->autosave.timeout = atoi (value);
      else if (strcmp (key, "auto_save_keystrokes") == 0)
        config->autosave.keystrokes = atoi (value);
      // Display
      else if (strcmp (key, "tab_width") == 0)
        config->display.tab_width = atoi (value);
    }
  fclose (file);
  return CONFIG_SUCCESS;
}