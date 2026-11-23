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
  /* C nests with brackets, which the scanner always knows: () [] {}.
   * Word pairs are for languages that nest with words, so C has none. */
  config->syntax.paired_keywords[0] = '\0';

  /* The languages led knows. The first is C, and `syntax` above holds
   * its words until a file chooses another. */
  memset (config->languages, 0, sizeof config->languages);
  Language *c = &config->languages[0];
  strcpy (c->name, "c");
  strcpy (c->extensions, config->syntax.extensions);
  strcpy (c->reserved_words, config->syntax.reserved_words);
  strcpy (c->line_comment, "//");
  strcpy (c->block_open, "/*");
  strcpy (c->block_close, "*/");
  strcpy (c->quotes, "\"'");
  c->escapes = 1;
  c->highlight = 1;

  /* FIRST (github.com/refgift/first): upper-case words, BEGIN ... END
   * and IF ... THEN nest, a denotation is "...", and there are no
   * comments and no escapes. */
  Language *f = &config->languages[1];
  strcpy (f->name, "first");
  strcpy (f->extensions, ".fst");
  strcpy (f->reserved_words,
          "BEGIN,END,IF,THEN,GOTO,JUMP,RETURN,INTEGER,DENOTED,INPUT,OUTPUT,"
          "YIELD,RANDOM,ROOT,COUNT,TIMED,GET,PUT,IN,OUT,TRUE,FALSE,"
          "SPACE,TAB,NEWLINE,CR,"
          "RED,BLUE,GREEN,WHITE,BLACK,CLEAR,YELLOW,MAGENTA,CYAN,BLANK,"
          "LEFT,RIGHT,UP,DOWN,HOME");
  strcpy (f->paired_keywords, "BEGIN-END,IF-THEN");
  strcpy (f->quotes, "\"");
  f->escapes = 0;
  f->highlight = 1;

  /* Plain text: nothing is code, so nothing is coloured. */
  Language *t = &config->languages[2];
  strcpy (t->name, "text");
  strcpy (t->extensions, ".txt,.text,.md");
  t->highlight = 0;

  /* Shell. "#" is a comment only where a word could start, so "$#" and
   * "${x#y}" are not. if-fi, do-done and case-esac nest. */
  Language *sh = &config->languages[3];
  strcpy (sh->name, "shell");
  strcpy (sh->extensions, ".sh,.bash");
  strcpy (sh->reserved_words,
          "if,then,else,elif,fi,for,while,until,do,done,case,esac,in,select,"
          "function,time,return,break,continue,exit,local,export,readonly,"
          "declare,typeset,unset,shift,source,alias,eval,exec,trap,set,"
          "echo,printf,read,cd,test,true,false,wait,getopts");
  strcpy (sh->paired_keywords, "if-fi,do-done,case-esac");
  strcpy (sh->line_comment, "#");
  strcpy (sh->quotes, "\"'");
  sh->escapes = 1;
  sh->highlight = 1;
  sh->comment_starts_word = 1;
  strcpy (sh->indent_after, "then,do,else,elif,in");
  strcpy (sh->interpreters, "sh,bash,dash,ksh,zsh,ash");

  /* lex / flex. The actions and the user code are C, so it reads as C
   * with flex's own names added. A backslash hides the next character,
   * which is what keeps \" and \( in a pattern from being taken for a
   * string or a bracket. */
  Language *lx = &config->languages[4];
  strcpy (lx->name, "lex");
  strcpy (lx->extensions, ".l,.lex");
  snprintf (lx->reserved_words, sizeof lx->reserved_words, "%s,%s",
            "int,char,return,if,else,for,while,do,switch,case,default,break,"
            "continue,goto,sizeof,typedef,struct,union,enum,static,extern,"
            "const,signed,unsigned,short,long,double,float,void,bool,true,false",
            "yytext,yyleng,yylex,yylval,yyin,yyout,yywrap,yymore,yyless,"
            "yyterminate,yyrestart,yylineno,ECHO,REJECT,BEGIN,INITIAL,YY_START,"
            "unput,input,option,noyywrap");
  strcpy (lx->line_comment, "//");
  strcpy (lx->block_open, "/*");
  strcpy (lx->block_close, "*/");
  strcpy (lx->quotes, "\"'");
  lx->escapes = 1;
  lx->highlight = 1;

  config->num_languages = 5;
  config->active_language = 0;
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
  config->display.system_clipboard = 1;
  config->display.persistent_undo = 1;
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

const Language *
config_language (const EditorConfig *config)
{
  /* A config no file has chosen a language for (tests build these by
   * hand) is read as C, which is what led did before it had languages. */
  static const Language c_rules = {
    .name = "c", .line_comment = "//", .block_open = "/*",
    .block_close = "*/", .quotes = "\"'", .escapes = 1, .highlight = 1
  };
  if (config && config->active_language >= 0
      && config->active_language < config->num_languages)
    return &config->languages[config->active_language];
  return &c_rules;
}

/* True if ext (".c") is one of the comma-separated extensions. */
static int
extension_listed (const char *list, const char *ext)
{
  size_t elen = strlen (ext);
  const char *p = list;
  while (*p)
    {
      const char *comma = strchr (p, ',');
      size_t tlen = comma ? (size_t) (comma - p) : strlen (p);
      while (tlen > 0 && (*p == ' ' || *p == '\t'))
        {
          p++;
          tlen--;
        }
      while (tlen > 0 && (p[tlen - 1] == ' ' || p[tlen - 1] == '\t'))
        tlen--;
      if (tlen == elen && memcmp (p, ext, elen) == 0)
        return 1;
      if (!comma)
        break;
      p = comma + 1;
    }
  return 0;
}

int
config_word_listed (const char *list, const char *word, int word_len)
{
  if (!list || !word || word_len <= 0)
    return 0;
  const char *p = list;
  while (*p)
    {
      const char *comma = strchr (p, ',');
      int tlen = comma ? (int) (comma - p) : (int) strlen (p);
      while (tlen > 0 && (*p == ' ' || *p == '\t'))
        {
          p++;
          tlen--;
        }
      while (tlen > 0 && (p[tlen - 1] == ' ' || p[tlen - 1] == '\t'))
        tlen--;
      if (tlen == word_len && memcmp (p, word, (size_t) word_len) == 0)
        return 1;
      if (!comma)
        break;
      p = comma + 1;
    }
  return 0;
}

/* Make language i the one in use. */
static const Language *
use_language (EditorConfig *config, int i)
{
  Language *l = &config->languages[i];
  config->active_language = i;
  snprintf (config->syntax.extensions, sizeof config->syntax.extensions,
            "%s", l->extensions);
  snprintf (config->syntax.reserved_words,
            sizeof config->syntax.reserved_words, "%s", l->reserved_words);
  snprintf (config->syntax.paired_keywords,
            sizeof config->syntax.paired_keywords, "%s", l->paired_keywords);
  config_parse_syntax (config);
  return l;
}

const Language *
config_select_language (EditorConfig *config, const char *filename)
{
  if (!config || !filename)
    return NULL;
  const char *slash = strrchr (filename, '/');
  const char *ext = strrchr (slash ? slash + 1 : filename, '.');
  if (!ext)
    return NULL;
  for (int i = 0; i < config->num_languages; i++)
    if (extension_listed (config->languages[i].extensions, ext))
      return use_language (config, i);
  return NULL;
}

const Language *
config_select_by_first_line (EditorConfig *config, const char *first_line)
{
  if (!config || !first_line || first_line[0] != '#' || first_line[1] != '!')
    return NULL;
  /* The interpreter is the last word of the path: "/bin/sh". With env
   * it is the word after it: "/usr/bin/env bash". */
  const char *p = first_line + 2;
  for (int word = 0; word < 2; word++)
    {
      while (*p == ' ' || *p == '\t')
        p++;
      const char *end = p;
      while (*end && *end != ' ' && *end != '\t')
        end++;
      const char *base = end;
      while (base > p && base[-1] != '/')
        base--;
      int n = (int) (end - base);
      if (n == 0)
        return NULL;
      if (!(n == 3 && memcmp (base, "env", 3) == 0))
        {
          for (int i = 0; i < config->num_languages; i++)
            if (config_word_listed (config->languages[i].interpreters, base, n))
              return use_language (config, i);
          return NULL;
        }
      p = end;
    }
  return NULL;
}

/* The language called name, made if there is room and it is new. */
static Language *
language_named (EditorConfig *config, const char *name)
{
  for (int i = 0; i < config->num_languages; i++)
    if (strcmp (config->languages[i].name, name) == 0)
      return &config->languages[i];
  if (config->num_languages >= LED_MAX_LANGUAGES
      || strlen (name) >= sizeof config->languages[0].name)
    return NULL;
  Language *l = &config->languages[config->num_languages++];
  memset (l, 0, sizeof *l);
  strcpy (l->name, name);
  strcpy (l->quotes, "\"");
  l->escapes = 1;
  l->highlight = 1;
  return l;
}

/* One "NAME.KEY=value" line. Returns 1 if it was a language key. */
static int
language_key (EditorConfig *config, const char *key, const char *value)
{
  const char *dot = strchr (key, '.');
  if (!dot || dot == key || !dot[1])
    return 0;
  char name[32];
  size_t n = (size_t) (dot - key);
  if (n >= sizeof name)
    return 0;
  memcpy (name, key, n);
  name[n] = '\0';
  const char *what = dot + 1;
  static const char *known[] = {
    "extensions", "reserved_words", "paired_keywords", "line_comment",
    "block_comment", "quotes", "escapes", "highlight",
    "comment_starts_word", "indent_after", "interpreters", NULL
  };
  int is_known = 0;
  for (int i = 0; known[i]; i++)
    if (strcmp (what, known[i]) == 0)
      is_known = 1;
  if (!is_known)
    return 0;
  Language *l = language_named (config, name);
  if (!l)
    return 1;
  if (strcmp (what, "extensions") == 0)
    snprintf (l->extensions, sizeof l->extensions, "%s", value);
  else if (strcmp (what, "reserved_words") == 0)
    snprintf (l->reserved_words, sizeof l->reserved_words, "%s", value);
  else if (strcmp (what, "paired_keywords") == 0)
    snprintf (l->paired_keywords, sizeof l->paired_keywords, "%s", value);
  else if (strcmp (what, "line_comment") == 0)
    snprintf (l->line_comment, sizeof l->line_comment, "%s", value);
  else if (strcmp (what, "block_comment") == 0)
    {
      /* "OPEN CLOSE", for example: slash-star star-slash. Empty for none. */
      char open[8] = "", close[8] = "";
      if (sscanf (value, "%7s %7s", open, close) == 2)
        {
          strcpy (l->block_open, open);
          strcpy (l->block_close, close);
        }
      else
        l->block_open[0] = l->block_close[0] = '\0';
    }
  else if (strcmp (what, "quotes") == 0)
    snprintf (l->quotes, sizeof l->quotes, "%s", value);
  else if (strcmp (what, "escapes") == 0)
    l->escapes = atoi (value) ? 1 : 0;
  else if (strcmp (what, "highlight") == 0)
    l->highlight = atoi (value) ? 1 : 0;
  else if (strcmp (what, "comment_starts_word") == 0)
    l->comment_starts_word = atoi (value) ? 1 : 0;
  else if (strcmp (what, "indent_after") == 0)
    snprintf (l->indent_after, sizeof l->indent_after, "%s", value);
  else if (strcmp (what, "interpreters") == 0)
    snprintf (l->interpreters, sizeof l->interpreters, "%s", value);
  return 1;
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
      /* These three, with no language name, are C's: they are older
       * than languages, and c.extensions and so on mean the same. */
      else if (strcmp (key, "syntax_extensions") == 0)
        {
          snprintf (config->syntax.extensions, sizeof (config->syntax.extensions), "%s", value);
          snprintf (config->languages[0].extensions, sizeof (config->languages[0].extensions), "%s", value);
        }
      else if (strcmp (key, "reserved_words") == 0)
        {
          snprintf (config->syntax.reserved_words, sizeof (config->syntax.reserved_words), "%s", value);
          snprintf (config->languages[0].reserved_words, sizeof (config->languages[0].reserved_words), "%s", value);
        }
      else if (strcmp (key, "paired_keywords") == 0)
        {
          snprintf (config->syntax.paired_keywords, sizeof (config->syntax.paired_keywords), "%s", value);
          snprintf (config->languages[0].paired_keywords, sizeof (config->languages[0].paired_keywords), "%s", value);
        }
      else if (language_key (config, key, value))
        {
          /* A change to C under its name also reaches the words in use. */
          snprintf (config->syntax.extensions, sizeof (config->syntax.extensions), "%s", config->languages[0].extensions);
          snprintf (config->syntax.reserved_words, sizeof (config->syntax.reserved_words), "%s", config->languages[0].reserved_words);
          snprintf (config->syntax.paired_keywords, sizeof (config->syntax.paired_keywords), "%s", config->languages[0].paired_keywords);
        }
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
      else if (strcmp (key, "system_clipboard") == 0)
        config->display.system_clipboard = atoi (value) ? 1 : 0;
      else if (strcmp (key, "persistent_undo") == 0)
        config->display.persistent_undo = atoi (value) ? 1 : 0;
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
           "# A longer line is loaded whole, but shown without syntax colours.\n"
           "#max_line_length=%d\n\n", d->performance.max_file_size_mb,
           d->performance.memory_limit_mb, d->performance.max_line_length);
  fprintf (f, "# Auto-save\n#auto_save_timeout=%d\n#auto_save_keystrokes=%d\n\n",
           d->autosave.timeout, d->autosave.keystrokes);
  fprintf (f, "# Search\n#search_enabled=%d\n#search_max_pattern_length=%d\n\n",
           d->search.enabled, d->search.max_pattern_length);
  fprintf (f, "# Copy and cut also go to the system clipboard (OSC 52).\n"
           "#system_clipboard=%d\n\n", d->display.system_clipboard);
  fprintf (f, "# Keep each file's undo history between sessions. It is stored in a\n"
           "# directory of your own under the system's temporary directory.\n"
           "#persistent_undo=%d\n", d->display.persistent_undo);
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
           "#syntax_extensions=%s\n#reserved_words=%s\n",
           d->syntax.extensions, d->syntax.reserved_words);
  fprintf (f, "\n# Brackets () [] {} are always coloured by nesting depth. Word pairs\n"
           "# are for a language that nests with words, written open-close.\n"
           "#paired_keywords=%s\n", d->syntax.paired_keywords);
  fprintf (f, "\n# Languages. A file's extension chooses one. The three keys above are\n"
           "# C's. Any language, these or one of your own, is set with NAME.KEY:\n"
           "#   NAME.extensions       .py,.pyw\n"
           "#   NAME.reserved_words   def,class,return\n"
           "#   NAME.paired_keywords  BEGIN-END,IF-THEN\n"
           "#   NAME.line_comment     #            (empty: the language has none)\n"
           "#   NAME.block_comment    /* */        (open, a space, close; empty: none)\n"
           "#   NAME.quotes           \"'           (each character that opens a string)\n"
           "#   NAME.escapes          1            (a backslash escapes inside a string)\n"
           "#   NAME.highlight        1            (0: shown as plain text)\n"
           "#   NAME.comment_starts_word 1         (the comment mark counts only at the\n"
           "#                                       start of a word: # but not $#)\n"
           "#   NAME.indent_after     then,do      (Enter indents after these words)\n"
           "#   NAME.interpreters     sh,bash      (chosen by a first line #!/bin/sh)\n");
  for (int i = 1; i < d->num_languages; i++)
    {
      const Language *l = &d->languages[i];
      fprintf (f, "\n#%s.extensions=%s\n#%s.reserved_words=%s\n"
               "#%s.paired_keywords=%s\n#%s.line_comment=%s\n"
               "#%s.block_comment=%s%s%s\n#%s.quotes=%s\n#%s.escapes=%d\n"
               "#%s.highlight=%d\n#%s.comment_starts_word=%d\n"
               "#%s.indent_after=%s\n#%s.interpreters=%s\n",
               l->name, l->extensions, l->name,
               l->reserved_words, l->name, l->paired_keywords, l->name,
               l->line_comment, l->name, l->block_open,
               l->block_open[0] ? " " : "", l->block_close, l->name,
               l->quotes, l->name, l->escapes, l->name, l->highlight,
               l->name, l->comment_starts_word, l->name, l->indent_after,
               l->name, l->interpreters);
    }
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
