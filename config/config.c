#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>
/* A colour from colorization.conf. Accepted, in any letter case:
 *   BLACK RED GREEN YELLOW BLUE MAGENTA CYAN WHITE   the eight standard ones
 *   BRIGHT_BLUE, LIGHT_BLUE, BRIGHTBLUE, LIGHTBLUE   ANSI extended colours 8-15
 *   GRAY / GREY                                      extended colour 8
 *   DEFAULT                                          the terminal's own colour
 *   0 .. 255                                         a colour by number
 * curses has no standard names past the first eight, so the bright
 * names are led's own. Anything else is WHITE. */
int
string_to_color (const char *str)
{
  static const char *names[8] = {
    "BLACK", "RED", "GREEN", "YELLOW", "BLUE", "MAGENTA", "CYAN", "WHITE"
  };
  char up[32];
  size_t n = 0;
  if (!str)
    return LED_COLOR_WHITE;
  while (*str == ' ' || *str == '\t')
    str++;
  for (; str[n] && n < sizeof up - 1; n++)
    up[n] = (char) toupper ((unsigned char) str[n]);
  up[n] = '\0';
  while (n > 0 && (up[n - 1] == ' ' || up[n - 1] == '\t'))
    up[--n] = '\0';
  if (n == 0)
    return LED_COLOR_WHITE;

  if (strcmp (up, "DEFAULT") == 0)
    return LED_COLOR_DEFAULT;
  if (strcmp (up, "GRAY") == 0 || strcmp (up, "GREY") == 0)
    return LED_COLOR_BRIGHT + LED_COLOR_BLACK;

  int all_digits = 1;
  for (size_t i = 0; i < n; i++)
    if (!isdigit ((unsigned char) up[i]))
      all_digits = 0;
  if (all_digits)
    {
      long v = strtol (up, NULL, 10);
      return (v >= 0 && v <= 255) ? (int) v : LED_COLOR_WHITE;
    }

  const char *base = up;
  int bright = 0;
  if (strncmp (up, "BRIGHT", 6) == 0)
    {
      base = up + 6;
      bright = 1;
    }
  else if (strncmp (up, "LIGHT", 5) == 0)
    {
      base = up + 5;
      bright = 1;
    }
  if (bright && (*base == '_' || *base == '-' || *base == ' '))
    base++;
  for (int c = 0; c < 8; c++)
    if (strcmp (base, names[c]) == 0)
      return bright ? LED_COLOR_BRIGHT + c : c;
  return LED_COLOR_WHITE;           // default
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
void
set_default_config (EditorConfig *config)
{
  config->version = 2;
  config->last_modified = time (NULL);
  config->last_error = CONFIG_SUCCESS;
  // Colors
  config->colors.normal_fg = LED_COLOR_WHITE;
  config->colors.normal_bg = LED_COLOR_BLACK;
  config->colors.selection_fg = LED_COLOR_BLACK;
  config->colors.selection_bg = LED_COLOR_WHITE;
  config->colors.semicolon_fg = LED_COLOR_RED;
  config->colors.semicolon_bg = LED_COLOR_BLACK;
  config->colors.meta_level1_fg = LED_COLOR_BLUE;
  config->colors.meta_level1_bg = LED_COLOR_BLACK;
  config->colors.meta_level2_fg = LED_COLOR_CYAN;
  config->colors.meta_level2_bg = LED_COLOR_BLACK;
  config->colors.meta_level3_fg = LED_COLOR_GREEN;
  config->colors.meta_level3_bg = LED_COLOR_BLACK;
  config->colors.meta_level4_fg = LED_COLOR_YELLOW;
  config->colors.meta_level4_bg = LED_COLOR_BLACK;
  config->colors.reserved_words_fg = LED_COLOR_RED;
  config->colors.reserved_words_bg = LED_COLOR_BLACK;
  // Syntax
  strcpy (config->syntax.extensions,
          ".c,.h,.C,.H,.cpp,.cc,.cxx,.hpp,.hh,.hxx");
  strcpy (config->syntax.reserved_words,
          "int,char,return,if,else,for,while,do,switch,case,default,break,continue,goto,sizeof,typedef,struct,union,enum,static,extern,auto,register,volatile,const,signed,unsigned,short,long,double,float,void,restrict,bool,true,false,nullptr,class,namespace,template,typename,public,private,protected,virtual,new,delete,try,catch,throw,using,operator,friend,inline,explicit,constexpr,noexcept,override,this,wchar_t,static_assert,concept,requires,co_await,co_return,co_yield,export,import,module,mutable,typeid,decltype");
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
  config->display.line_numbers_configured = 0;
  config->display.syntax_highlight = 0;
  config->display.tab_width = 8;
  config->display.spaces_for_tab = 0;
  config->display.word_wrap = 0;  /* default off for stability */
  config->display.show_border = 0; /* default off; LED_SHOW_BORDER=1 to show, F4 toggles; hides for xterm copy */
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

int config_is_reserved_word (const EditorConfig *config, const char *word);

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

  /* A pair is a pair only if both words belong to the language or both
   * are outside its reserved words. "if-then" with "if" reserved and
   * "then" not is C written with a Pascal pair: the "then" never comes,
   * so every "if" would open a level that nothing closes. Leave such a
   * pair out and let the word be coloured as the reserved word it is. */
  int kept = 0;
  for (int p = 0; p < config->num_syntax_pairs; p++)
    {
      SyntaxPair *pair = &config->syntax_pairs[p];
      int open_reserved = config_is_reserved_word (config, pair->open);
      int close_reserved = config_is_reserved_word (config, pair->close);
      if (open_reserved == close_reserved)
        config->syntax_pairs[kept++] = *pair;
    }
  config->num_syntax_pairs = kept;
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

/* Where the config files live. "--prefix DIR" replaces "$HOME/.config",
 * for systems that keep settings somewhere else; the files are then
 * DIR/led/led.conf and DIR/led/colorization.conf. */
static char g_prefix[400];

void
config_set_prefix (const char *dir)
{
  snprintf (g_prefix, sizeof g_prefix, "%s", dir ? dir : "");
  size_t n = strlen (g_prefix);
  while (n > 1 && g_prefix[n - 1] == '/')
    g_prefix[--n] = '\0';
}

/* The led config directory, or 0 when there is nowhere to look. */
static int
config_dir (char *out, size_t size)
{
  if (g_prefix[0])
    return snprintf (out, size, "%s/led", g_prefix) < (int) size;
  const char *home = getenv ("HOME");
  if (!home || !*home)
    return 0;
  return snprintf (out, size, "%s/.config/led", home) < (int) size;
}

/* Read one file of key=value lines. Every key is accepted in either
 * file, so a colorization.conf written before led.conf existed still
 * sets everything it used to. A missing file changes nothing. */
static void
config_read_file (EditorConfig *config, const char *path)
{
  FILE *file = fopen (path, "r");
  if (!file)
    return;
  char line[4096];
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
      else if (strcmp (key, "show_line_numbers") == 0)
        {
          config->display.show_line_numbers = atoi (value);
          config->display.line_numbers_configured = 1;
        }
      else if (strcmp (key, "show_key_meter") == 0)
        config->statusbar.show_key_meter = atoi (value);
      else if (strcmp (key, "max_file_size_mb") == 0)
        {
          int v = atoi (value);
          if (v >= 1 && v <= 100)
            config->performance.max_file_size_mb = v;
        }
      else if (strcmp (key, "memory_limit_mb") == 0)
        {
          int v = atoi (value);
          if (v >= 1 && v <= 200)
            config->performance.memory_limit_mb = v;
        }
      else if (strcmp (key, "max_line_length") == 0)
        {
          int v = atoi (value);
          if (v >= 80 && v <= 100000)
            config->performance.max_line_length = v;
        }
    }
  fclose (file);
}

ConfigError
load_editor_config (EditorConfig *config)
{
  set_default_config (config);
  char dir[512];
  if (config_dir (dir, sizeof dir))
    {
      char path[600];
      /* led.conf is read last, so what it sets wins. */
      snprintf (path, sizeof path, "%s/colorization.conf", dir);
      config_read_file (config, path);
      snprintf (path, sizeof path, "%s/led.conf", dir);
      config_read_file (config, path);
    }
  config_parse_syntax (config);
  config_apply_env_overrides (config);
  return CONFIG_SUCCESS;
}

static const char *const color_names[8] = {
  "BLACK", "RED", "GREEN", "YELLOW", "BLUE", "MAGENTA", "CYAN", "WHITE"
};

static const char *
color_to_string (int c, char *buf, size_t size)
{
  if (c == LED_COLOR_DEFAULT)
    return "DEFAULT";
  if (c >= 0 && c < 8)
    return color_names[c];
  if (c >= LED_COLOR_BRIGHT && c < LED_COLOR_BRIGHT + 8)
    snprintf (buf, size, "LIGHT_%s", color_names[c - LED_COLOR_BRIGHT]);
  else
    snprintf (buf, size, "%d", c);
  return buf;
}

/* Create path with the given text unless something is already there. */
static int
write_new_file (const char *path, void (*body) (FILE *, const EditorConfig *),
                const EditorConfig *d)
{
  FILE *f = fopen (path, "wx");
  if (!f)
    return 0;
  body (f, d);
  return fclose (f) == 0;
}

static void
general_body (FILE *f, const EditorConfig *d)
{
  fprintf (f, "# led.conf - general settings for led.\n"
           "# Each line below shows the built-in default. Remove the leading '#'\n"
           "# and change the value to override it. Colours and syntax words are\n"
           "# in colorization.conf, next to this file.\n\n");
  fprintf (f, "# Display\n#tab_width=%d\n#spaces_for_tab=%d\n#show_border=%d\n"
           "#show_key_meter=%d\n", d->display.tab_width,
           d->display.spaces_for_tab, d->display.show_border,
           d->statusbar.show_key_meter);
  fprintf (f, "# Source files show line numbers and other files do not, unless this is set.\n"
           "#show_line_numbers=%d\n\n", d->display.show_line_numbers);
  fprintf (f, "# Limits\n#max_file_size_mb=%d\n#memory_limit_mb=%d\n"
           "#max_line_length=%d\n\n", d->performance.max_file_size_mb,
           d->performance.memory_limit_mb, d->performance.max_line_length);
  fprintf (f, "# Auto-save\n#auto_save_timeout=%d\n#auto_save_keystrokes=%d\n\n",
           d->autosave.timeout, d->autosave.keystrokes);
  fprintf (f, "# Search\n#search_enabled=%d\n#search_max_pattern_length=%d\n",
           d->search.enabled, d->search.max_pattern_length);
}

static void
color_body (FILE *f, const EditorConfig *d)
{
  char a[32], b[32];
  const ColorScheme *c = &d->colors;
  fprintf (f, "# colorization.conf - colours and syntax words for led.\n"
           "# Each line below shows the built-in default. Remove the leading '#'\n"
           "# and change the value to override it. General settings are in\n"
           "# led.conf, next to this file.\n#\n"
           "# Colours: the eight curses colours BLACK RED GREEN YELLOW BLUE\n"
           "# MAGENTA CYAN WHITE (0-7). The ANSI extended colours 8-15, on a\n"
           "# terminal that has them, are LIGHT_BLUE (or BRIGHT_BLUE) and so on,\n"
           "# and GRAY. DEFAULT is the terminal's own colour. A number 0-255\n"
           "# also works.\n\n");
#define PAIR(name) \
  fprintf (f, "#" #name "_fg=%s\n#" #name "_bg=%s\n", \
           color_to_string (c->name##_fg, a, sizeof a), \
           color_to_string (c->name##_bg, b, sizeof b))
  PAIR (normal);
  PAIR (selection);
  PAIR (semicolon);
  PAIR (reserved_words);
  PAIR (meta_level1);
  PAIR (meta_level2);
  PAIR (meta_level3);
  PAIR (meta_level4);
#undef PAIR
  fprintf (f, "\n# Files with these extensions are highlighted.\n"
           "#syntax_extensions=%s\n#reserved_words=%s\n#paired_keywords=%s\n",
           d->syntax.extensions, d->syntax.reserved_words,
           d->syntax.paired_keywords);
}

int
config_write_defaults (void)
{
  char dir[512];
  if (!config_dir (dir, sizeof dir))
    return 0;
  /* Make each directory on the way; one that exists is fine. */
  for (char *p = dir + 1; ; p++)
    if (*p == '/' || *p == '\0')
      {
        char keep = *p;
        *p = '\0';
        (void) mkdir (dir, 0755);
        *p = keep;
        if (!keep)
          break;
      }
  EditorConfig *d = malloc (sizeof *d);
  if (!d)
    return 0;
  set_default_config (d);
  char path[600];
  int made = 0;
  snprintf (path, sizeof path, "%s/led.conf", dir);
  made += write_new_file (path, general_body, d);
  snprintf (path, sizeof path, "%s/colorization.conf", dir);
  made += write_new_file (path, color_body, d);
  free (d);
  return made;
}
