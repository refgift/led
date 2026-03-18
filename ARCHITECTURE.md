# LED Editor - Architecture Overview

## System Architecture

```
┌─────────────────────────────────────────────────────────┐
│                    MAIN EVENT LOOP                       │
│                    (main.c:78-85)                        │
├─────────────────────────────────────────────────────────┤
│                                                           │
│  while (1) {                                             │
│    int ch = getch();              ← Read keyboard input  │
│    if (ch == 17) break;           ← Ctrl+Q to exit      │
│    editor_handle_input(&ed, ch);  ← Process input       │
│    editor_draw(stdscr, &ed);      ← Render display      │
│  }                                                        │
│                                                           │
└─────────────────────────────────────────────────────────┘
         ↑                                    ↓
         │                                    │
    (main.c:80)                          (editor.c:130)
         │                                    │
         ↓                                    ↓
┌─────────────────────┐             ┌──────────────────────┐
│   Keyboard Input    │             │   Render Display     │
│   (getch)           │             │   (editor_draw)      │
└─────────────────────┘             └──────────────────────┘
                                             ↓
                                     (view.c:560-884)
                                             │
                                    ┌────────────────────┐
                                    │   draw_update()    │
                                    │                    │
                                    │  Check word_wrap   │
                                    │  Render lines      │
                                    │  Status bar        │
                                    └────────────────────┘
```

---

## F3 Key Handling Flow

```
                    F3 Pressed
                        │
                        ↓
            main.c:80 → getch() = KEY_F(3)
                        │
                        ↓
        main.c:83 → editor_handle_input(&ed, KEY_F(3))
                        │
        ┌───────────────┴───────────────┐
        │                               │
  editor.c:145               editor.c:150-154
  (Check if F2)              (Check if F3)
        │                        │
        ↓                        ↓
   Toggle                  Toggle
   line_numbers         word_wrap flag
        │                        │
        ↓                        ↓
   config.display.         config.display.
   word_wrap unchanged      word_wrap = !word_wrap
        │                        │
        └───────────────┬────────┘
                        ↓
            main.c:84 → editor_draw()
                        │
                        ↓
            editor.c:130 → draw_update()
                        │
        ┌───────────────┴────────────────┐
        │                                │
    word_wrap OFF              word_wrap ON
    (view.c:691-742)           (view.c:614-690)
        │                            │
        ├─ Truncate lines           ├─ Wrap long lines
        ├─ One row per line         ├─ Multiple rows
        ├─ Horiz scroll on          ├─ Horiz scroll off
        └─ Line counting            └─ Line counting
            unchanged                   unchanged
```

---

## Data Structure Hierarchy

```
┌─────────────────────────────────────────────────────────────┐
│                         Editor                              │
│                      (editor.h:8-44)                        │
├─────────────────────────────────────────────────────────────┤
│                                                              │
│  Buffer model;                    ← Text data               │
│  int scroll_row;                  ← Visible range           │
│  int scroll_col;                  ← (when word_wrap OFF)    │
│  int cursor_line, cursor_col;     ← Cursor position         │
│  int show_line_numbers;           ← F2 toggle              │
│  EditorConfig config;             ← Settings               │
│  ├─ DisplayConfig display;        ← word_wrap flag         │
│  ├─ ColorScheme colors;                                     │
│  ├─ SyntaxConfig syntax;                                    │
│  └─ StatusBarConfig statusbar;                             │
│                                                              │
│  Selection fields;                ← For highlighting        │
│  Search/replace buffers;          ← Find & replace          │
│  Auto-save fields;                ← Save management         │
│  char status_message[256];        ← Temporary feedback      │
│                                                              │
└─────────────────────────────────────────────────────────────┘
              │
              ├─────────────────┐
              ↓                 ↓
    ┌──────────────┐  ┌─────────────────────┐
    │   Buffer     │  │  EditorConfig       │
    │ (model.h)    │  │  (config.h)         │
    ├──────────────┤  ├─────────────────────┤
    │              │  │                     │
    │ char** lines │  │ DisplayConfig       │
    │ int num_lnes │  │ ├─ word_wrap       │
    │ int capacity │  │ ├─ tab_width       │
    │              │  │ ├─ show_line_nums  │
    └──────────────┘  │ └─ syntax_highlight│
                      │                     │
                      │ ColorScheme         │
                      │ SyntaxConfig        │
                      │ AutoSaveConfig      │
                      │ StatusBarConfig     │
                      │ PerformanceConfig   │
                      │                     │
                      └─────────────────────┘
```

---

## View Rendering Pipeline

```
┌──────────────────────────────────────────────────────────────┐
│           draw_update() (view.c:558-884)                     │
└──────────────────────────────────────────────────────────────┘
                          │
                ┌─────────┴─────────┐
                ↓                   ↓
        Clear & Box        Adjust scroll
        (line 586)         (lines 569-585)
                            │
                ┌───────────┴───────────┐
                ↓                       ↓
        Calculate width        Main render loop
        (lines 588-590)        (lines 592-745)
                               │
                    ┌──────────┴──────────┐
                    ↓                     ↓
            word_wrap ON?          word_wrap OFF?
                 │                        │
        ┌────────┴────────┐      ┌────────┴────────┐
        ↓                 ↓      ↓                 ↓
    WRAP PATH         Default   TRUNCATE PATH   Default
    (614-690)         Path      (691-742)       Path
        │                           │
        ├─ While pos < len      ├─ Line number
        ├─ Find break point     ├─ Truncate to width
        ├─ Show line num        ├─ Render segment
        ├─ Render segment       └─ One visual row
        ├─ Respect selection        per logical line
        ├─ Apply colors
        └─ Increment visual_row
            for each segment
                │                     │
        ┌───────┴─────────┐    ┌─────┴──────────┐
        ↓                 ↓    ↓                ↓
    Segment rendering    Syntax highlighting
    respects:            enabled via
    ├─ Selection         print_highlighted()
    ├─ Colors            (view.c:355-467)
    └─ Tab expansion
                │                     │
                └──────────┬──────────┘
                           ↓
                ┌───────────────────────┐
                │   Status Bar Render   │
                │   (lines 746-858)     │
                ├───────────────────────┤
                │                       │
                │ Build status_line:   │
                │  ├─ Time (optional)  │
                │  ├─ Version (opt)    │
                │  ├─ Filename         │
                │  ├─ Position:        │
                │  │  Line X/Y Col Z   │
                │  └─ Modification *   │
                │                       │
                │ Render at line 858:  │
                │ mvprintw(LINES-1,..) │
                └───────────────────────┘
                           │
                           ↓
                    ┌───────────────────┐
                    │  Position cursor  │
                    │  (lines 860-883)  │
                    │                   │
                    │  Calculate:       │
                    │  ├─ screen_y      │
                    │  ├─ screen_x      │
                    │  └─ Clamp bounds  │
                    │                   │
                    │  move(y, x)       │
                    │  refresh()        │
                    └───────────────────┘
```

---

## Word Wrap Decision Tree

```
                    draw_update() called
                            │
        ┌───────────────────┴───────────────────┐
        ↓                                       ↓
  Check: config->display.word_wrap == 1?
        │                                       │
      YES                                       NO
        │                                       │
        ↓                                       ↓
    WRAP MODE                            TRUNCATE MODE
    (lines 614-690)                      (lines 691-742)
        │                                       │
        ├─ available_width =                ├─ available_width =
        │   COLS - 2 - num_width            │   COLS - 2 - num_width
        │                                   │
        ├─ pos = 0                         ├─ pos = scroll_col
        │   (ignore scroll_col)            │
        │                                   ├─ If len > available_width:
        ├─ While pos < len:                │    Print first available_width chars
        │   ├─ Find break point            │
        │   │   (word boundary)            ├─ Selection still highlighted
        │   │                              │
        │   ├─ If no space:                └─ Only one visual row
        │   │   Hard break                     per logical line
        │   │
        │   ├─ Print segment
        │   ├─ Apply selection highlighting
        │   ├─ Apply syntax highlighting
        │   ├─ visual_row++
        │   │
        │   └─ If visual_row >= max_lines:
        │       EXIT (buffer limit)
        │
        ├─ Line number shown ONCE
        │   (only on first segment)
        │
        └─ Multiple visual rows
            per logical line
```

---

## Line Count Calculation

```
┌──────────────────────────────────────────────────┐
│  buffer_num_lines(buf)  (model.c:282-286)       │
│  ────────────────────────────────────────────   │
│  Returns: buf->num_lines                        │
│  (Always the logical line count)                │
│  (NEVER affected by word_wrap setting)          │
└──────────────────────────────────────────────────┘
                    │
                    ├─ Used in status bar:
                    │  snprintf(pos_str, 64,
                    │   "Line %u/%u Col %u",
                    │    cursor_line + 1,
                    │    buffer_num_lines(buf),  ← HERE
                    │    cursor_col + 1);
                    │
                    ├─ Used for calculate width:
                    │  num_digits = calculate_digits(
                    │   buffer_num_lines(buf));
                    │
                    ├─ Used in rendering loop:
                    │  while (... && logical_line <
                    │   buffer_num_lines(buf)) { ... }
                    │
                    └─ Used in visual rows calc:
                       (view.c:100-117)
                       if (!config->display.word_wrap)
                           return buffer_num_lines(buf);
                       else
                           /* calculate wrapped total */
```

---

## Configuration Flow

```
┌────────────────────────────────────────┐
│   Program Start (main.c:13-48)         │
└────────────────────────────────────────┘
              │
              ↓
┌────────────────────────────────────────┐
│   editor_init (editor.c:11-102)        │
└────────────────────────────────────────┘
              │
              ↓
┌────────────────────────────────────────────────┐
│  load_editor_config() (config.c:91-172)        │
│  ────────────────────────────────────────────  │
│  1. Call set_default_config()                  │
│     ├─ config->display.word_wrap = 0 (OFF)    │
│     ├─ config->display.tab_width = 8          │
│     ├─ config->statusbar.enabled = 1          │
│     └─ ... other defaults                      │
│                                                │
│  2. Read ~/.config/led/colorization.conf       │
│     (if exists, override defaults)             │
└────────────────────────────────────────────────┘
              │
              ↓
┌────────────────────────────────────────┐
│  Editor object initialized with config │
│                                        │
│  ed->config.display.word_wrap = 0     │
│  (Ready for F3 toggle)                │
└────────────────────────────────────────┘
              │
        ┌─────┴─────┐
        ↓           ↓
   F3 pressed   draw_update()
        │           │
        ↓           ↓
   Toggle flag   Check flag
   (editor.c)    (view.c)
        │           │
        ↓           ↓
   Render with
   new value
```

---

## File Dependencies

```
                    main.c
                       │
        ┌──────────────┼──────────────┐
        ↓              ↓              ↓
    editor.h       view.h        config.h
        │              │              │
        ├─ model.h     ├─ model.h     ├─ ncurses.h
        ├─ config.h    ├─ config.h    ├─ time.h
        ├─ controller  ├─ editor.h    └─ (ncurses basics)
        └─ view.h      └─ (ncurses)
        │
    editor.c
    controller.c
    model.c
    
    Compile dependency graph:
    ├─ model.c (model.h) → model.o
    ├─ config.c (config.h) → config.o
    ├─ view.c (view.h, model.h, config.h) → view.o
    ├─ controller.c (controller.h, model.h) → controller.o
    ├─ editor.c (editor.h, *.h files) → editor.o
    └─ main.c (all .h files) + *.o files → led binary
```

---

## Memory Layout (Logical vs Visual)

```
LOGICAL BUFFER (Model Layer)
═════════════════════════════════════════════════════════════

Line 0: "The quick brown fox jumps over the lazy dog"
Line 1: "Hello world"
Line 2: "Long line that definitely exceeds terminal width xyz"
...

num_lines = 3


VISUAL DISPLAY with word_wrap OFF (Truncate)
═════════════════════════════════════════════════════════════

Screen Row 1: The quick brown fox jumps over the lazy →
Line 0       (truncated at column 80)

Screen Row 2: Hello world
Line 1       (fits on one row)

Screen Row 3: Long line that definitely exceeds terminal w →
Line 2       (truncated)


VISUAL DISPLAY with word_wrap ON (Wrap)
═════════════════════════════════════════════════════════════

Screen Row 1: The quick brown fox jumps over the lazy
Line 0
Screen Row 2: dog
(continued)

Screen Row 3: Hello world
Line 1       (still fits on one row)

Screen Row 4: Long line that definitely exceeds terminal
Line 2       (wrapped segment 1)
Screen Row 5: width xyz
(continued)

Screen Row 6: (continues...)


STATUS BAR (Always uses logical line count)
═════════════════════════════════════════════════════════════
Line 1/3 Col 45   ← Always shows 3 total lines
          ↑ ↑       (never changes with word_wrap)
          │ └─ cursor_col + 1
          └─ cursor_line + 1
```

