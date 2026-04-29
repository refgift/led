# LED Editor - Quick Reference Guide

## F3 Key Binding (Word Wrap Toggle)

**Where:** `/home/larry/Documents/refgift/led/editor.c` lines 150-154

```c
else if (ch == KEY_F (3))
{
    ed->config.display.word_wrap = !ed->config.display.word_wrap;
    return;
}
```

**Call chain:**
- `main.c:80` → `getch()` reads F3 key
- `main.c:83` → `editor_handle_input(ed, KEY_F(3))`
- `editor.c:150-154` → Toggle flag
- `main.c:84` → `editor_draw()` re-renders with new setting

---

## Line Count Display

**Storage:** `model.h` → `Buffer.num_lines` (int)

**Getter:** `model.c:282-286` → `buffer_num_lines()`
```c
int buffer_num_lines(const Buffer *buf)
{
    return buf->num_lines;
}
```

**Display:** `view.c:793-795` → Status bar shows:
```
Line <current>/<total> Col <column>
```

Example: `Line 5/150 Col 23`

---

## Word Wrap Rendering

**Main function:** `view.c:558-884` → `draw_update()`

**Decision point:** `view.c:614`

**If word_wrap ON (lines 614-690):**
- Split long logical lines into multiple visual rows
- Break at word boundaries (spaces)
- Hard break if no space found
- Line number shown once per logical line
- Selection highlighted across wrapped segments

**If word_wrap OFF (lines 691-742):**
- Truncate to screen width
- Only one visual row per logical line
- Horizontal scroll via `scroll_col`
- Selection highlighted on visible portion

---

## Helper Functions

**Calculate rows per line:**
- Function: `view.c:54-98` → `visual_rows_for_line()`
- Returns: How many screen rows needed for one logical line

**Calculate total visual lines:**
- Function: `view.c:100-117` → `calculate_total_visual_lines()`
- Returns: Total rows needed for entire buffer

---

## Configuration

**Struct:** `config.h:32-38` → `DisplayConfig`

```c
typedef struct {
    int show_line_numbers;      // F2 toggle
    int syntax_highlight;
    int word_wrap;              // F3 toggle (0=OFF, 1=ON)
    int tab_width;
    int spaces_for_tab;
} DisplayConfig;
```

**Default:** `word_wrap = 0` (initialized in `config.c:83`)

---

## Key Structures

**Editor state:** `editor.h:8-44`
- Contains: `config`, `scroll_row`, `scroll_col`, `cursor_line`, `cursor_col`
- Plus: selection, search, clipboard, auto-save fields

**Buffer model:** `model.h:8-12`
- `lines[]` - array of line strings
- `num_lines` - logical line count
- `capacity` - allocated capacity

---

## Status Bar Components

**File:** `view.c:746-858` (in `draw_update()`)

**Parts:**
1. Temporary message (5-sec feedback)
2. Search/Replace mode strings
3. Standard status:
   - Time (optional)
   - Version (optional)
   - Position: `Line X/Y Col Z`
   - Filename + modification asterisk
   - Two layout styles (centered/balanced)

**Rendered at:** `view.c:858` → Bottom row of terminal

---

## Test Command

```bash
cd /home/larry/Documents/refgift/led
./led -t                    # Run all tests
./led testfile.txt          # Run editor on file
```

**F3 test:** `test_controller.c:235-290` → `test_word_wrap_toggle()`

---

## File Locations Summary

| What | File | Line(s) |
|------|------|---------|
| F3 toggle | editor.c | 150-154 |
| Word wrap rendering | view.c | 614-742 |
| Line count getter | model.c | 282-286 |
| Status bar display | view.c | 793-795 |
| Status bar render | view.c | 858 |
| Config struct | config.h | 32-38 |
| Default config | config.c | 83 |
| Event loop | main.c | 78-85 |
| Visual rows calc | view.c | 54-98 |
| Total rows calc | view.c | 100-117 |

