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
  config->autosave.timeout = 80000000;
  config->autosave.keystrokes = 500;
  // Status bar
  config->statusbar.enabled = 1;
  config->statusbar.show_version = 1;
  config->statusbar.show_time = 0;
  config->statusbar.show_key_meter = 1;
  config->statusbar.time_format = 24;
  config->statusbar.style = 1;  // balanced
  // Display
  config->display.show_line_numbers = 0;
  config->display.syntax_highlight = 0;
  config->display.tab_width = 8;
  config->display.spaces_for_tab = 0;
  config->display.word_wrap = 0;  /* default off for stability */
  config->display.show_border = 0; /* default on; set 0 or LED_NO_BORDER=1 to hide for xterm copy */
  // Performance
  config->performance.max_file_size_mb = 10;
  config->performance.memory_limit_mb = 50;
  config->performance.max_line_length = 10000;
  config->search.enabled = 1;
  config->search.max_pattern_length = 100;
}
static void
config_apply_env_overrides (EditorConfig *config)
{
  const char *v = getenv ("LED_NO_BORDER");
  if (v && *v && strcmp (v, "0") != 0 && strcmp (v, "false") != 0)
    config->display.show_border = 0;
  v = getenv ("LED_SHOW_BORDER");
  if (v)
    config->display.show_border = atoi (v) ? 1 : 0;
}

void
config_parse_syntax (EditorConfig *config)
{
  config->num_syntax_pairs = 0;
  config->reserved_count = 0;
  if (!config)
    return;

  /* Parse paired keywords: comma-separated "open-close" tokens */
  const char *pk = config->syntax.paired_keywords;
  while (*pk && config->num_syntax_pairs < LED_MAX_SYNTAX_PAIRS)
    {
      const char *comma = strchr (pk, ',');
      size_t tlen = comma ? (size_t) (comma - pk) : strlen (pk);
      char token[96];
      if (tlen >= sizeof (token))
        tlen = sizeof (token) - 1;
      memcpy (token, pk, tlen);
      token[tlen] = '\0';
      char *dash = strchr (token, '-');
      if (dash && dash > token)
        {
          SyntaxPair *pair =
            &config->syntax_pairs[config->num_syntax_pairs];
          size_t olen = (size_t) (dash - token);
          if (olen >= sizeof (pair->open))
            olen = sizeof (pair->open) - 1;
          memcpy (pair->open, token, olen);
          pair->open[olen] = '\0';
          const char *close = dash + 1;
          snprintf (pair->close, sizeof (pair->close), "%s", close);
          trim_whitespace (pair->open);
          trim_whitespace (pair->close);
          if (pair->open[0] != '\0' && pair->close[0] != '\0')
            config->num_syntax_pairs++;
        }
      if (!comma)
        break;
      pk = comma + 1;
    }

  /* Parse reserved words into a sorted, NUL-packed array */
  char *store = config->reserved_sorted;
  size_t used = 0;
  const char *rw = config->syntax.reserved_words;
  while (*rw && config->reserved_count < 256
         && used < sizeof (config->reserved_sorted))
    {
      const char *comma = strchr (rw, ',');
      size_t tlen = comma ? (size_t) (comma - rw) : strlen (rw);
      while (tlen > 0 && (rw[0] == ' ' || rw[0] == '\t'))
        {
          rw++;
          tlen--;
        }
      while (tlen > 0 && (rw[tlen - 1] == ' ' || rw[tlen - 1] == '\t'))
        tlen--;
      if (tlen > 0)
        {
          if (used + tlen + 1 > sizeof (config->reserved_sorted))
            break;
          config->reserved_offset[config->reserved_count++] = (int) used;
          memcpy (store + used, rw, tlen);
          used += tlen;
          store[used++] = '\0';
        }
      if (!comma)
        break;
      rw = comma + 1;
    }

  /* Sort word indices by their strings (insertion sort; n <= 256) */
  for (int i = 1; i < config->reserved_count; i++)
    {
      int key = config->reserved_offset[i];
      int j = i - 1;
      while (j >= 0
             && strcmp (store + config->reserved_offset[j], store + key) > 0)
        {
          config->reserved_offset[j + 1] = config->reserved_offset[j];
          j--;
        }
      config->reserved_offset[j + 1] = key;
    }
}

int
config_is_reserved_word (const EditorConfig *config, const char *word)
{
  if (!config || !word || config->reserved_count == 0)
    return 0;
  int lo = 0;
  int hi = config->reserved_count - 1;
  while (lo <= hi)
    {
      int mid = lo + (hi - lo) / 2;
      const char *candidate =
        config->reserved_sorted + config->reserved_offset[mid];
      int cmp = strcmp (word, candidate);
      if (cmp == 0)
        return 1;
      if (cmp < 0)
        hi = mid - 1;
      else
        lo = mid + 1;
    }
  return 0;
}

ConfigError
load_editor_config (EditorConfig *config)
{
  set_default_config (config);
  const char *home = getenv ("HOME");
  if (!home)
    {
      config_parse_syntax (config);
      config_apply_env_overrides (config);
      return CONFIG_SUCCESS;
    }
  char path[512];
  snprintf (path, sizeof (path), "%s/.config/led/colorization.conf", home);
  FILE *file = fopen (path, "r");
  if (!file)
    {
      config_parse_syntax (config);
      config_apply_env_overrides (config);
      return CONFIG_SUCCESS;     // use defaults
    }
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
        snprintf (config->syntax.extensions, sizeof (config->syntax.extensions), "%s", value);
      else if (strcmp (key, "reserved_words") == 0)
        snprintf (config->syntax.reserved_words, sizeof (config->syntax.reserved_words), "%s", value);
      else if (strcmp (key, "paired_keywords") == 0)
        snprintf (config->syntax.paired_keywords, sizeof (config->syntax.paired_keywords), "%s", value);
      // Auto-save
      else if (strcmp (key, "auto_save_timeout") == 0)
        config->autosave.timeout = atoi (value);
      else if (strcmp (key, "auto_save_keystrokes") == 0)
        config->autosave.keystrokes = atoi (value);
      else if (strcmp (key, "search_enabled") == 0)
        config->search.enabled = atoi (value);
      else if (strcmp (key, "search_max_pattern_length") == 0)
        config->search.max_pattern_length = atoi (value);
      else if (strcmp (key, "tab_width") == 0)
        config->display.tab_width = atoi (value);
      else if (strcmp (key, "spaces_for_tab") == 0)
        config->display.spaces_for_tab = atoi (value);
      else if (strcmp (key, "show_border") == 0)
        config->display.show_border = atoi (value);
      else if (strcmp (key, "show_key_meter") == 0)
        config->statusbar.show_key_meter = atoi (value);
    }
  fclose (file);
  config_parse_syntax (config);
  config_apply_env_overrides (config);
  return CONFIG_SUCCESS;
}
