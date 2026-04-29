# LED Editor Codebase Search Results

## Summary
This is a C-based terminal text editor using ncurses. The codebase implements word wrap toggling (F3 key), line counting, and view rendering with preliminary word wrap support.

---

## 1. F3 KEY BINDING - WORD WRAP TOGGLE

### Location & Implementation
**File:** `/home/larry/Documents/refgift/led/editor.c`
**Function:** `editor_handle_input()` (lines 142-238)

**Code (lines 150-154):**
```c
else if (ch == KEY_F (3))
{
    ed->config.display.word_wrap = !ed->config.display.word_wrap;
    return;
}
```

### How it works:
1. The event loop in `main.c` (line 80) calls `getch()` to read keyboard input
2. Input character `ch` is passed to `editor_handle_input()` in `editor.c` (line 83)
3. The F3 key is detected as `KEY_F(3)` (ncurses macro)
4. When pressed, it toggles the boolean flag `ed->config.display.word_wrap`
5. The toggle is instantaneous - no buffer modification occurs
6. After toggle, control returns to main event loop which calls `editor_draw()` (line 84)

### Key Binding Entry Point:
- **Main event loop:** `/home/larry/Documents/refgift/led/main.c` line 78-85
- **Key handler:** `/home/larry/Documents/refgift/led/editor.c` lines 142-154
- **ncurses constant:** `KEY_F(3)` macro (from ncurses.h)

---

## 2. LINE COUNT - CALCULATION & DISPLAY

### Line Count Storage (Model Layer)
**File:** `/home/larry/Documents/refgift/led/model.h`
**Struct:** `Buffer` (lines 8-12)

```c
typedef struct {
    char** lines;      // Array of line strings
    int num_lines;     // ACTUAL LINE COUNT (logical lines)
    int capacity;      // Allocated capacity
} Buffer;
```

### Function to Get Line Count
**File:** `/home/larry/Documents/refgift/led/model.c`
**Function:** `buffer_num_lines()` (lines 282-286)

```c
int
buffer_num_lines (const Buffer *buf)
{
  return buf->num_lines;
}
```

### Where Line Count is Displayed (Status Bar)
**File:** `/home/larry/Documents/refgift/led/view.c`
**Function:** `draw_update()` (lines 558-884)

**Display Logic (lines 793-795):**
```c
char pos_str[64];
snprintf (pos_str, 64, "Line %u/%u Col %u",
         cursor_line + 1, buffer_num_lines (buf), cursor_col + 1);
```

**Status Bar Rendering (lines 746-858):**
- Displays as: `Line <current>/<total> Col <column>`
- Example: `Line 5/150 Col 23`
- Updated on every draw call in the main event loop

### Initialization
**File:** `/home/larry/Documents/refgift/led/config.c`
**Function:** `set_default_config()` (lines 44-89)
- `config->display.word_wrap` initialized to `0` (OFF) on line 83

---

## 3. VIEW RENDERING FOR WRAPPED vs UNWRAPPED TEXT

### Core Rendering Function
**File:** `/home/larry/Documents/refgift/led/view.c`
**Function:** `draw_update()` (lines 558-884)

#### Rendering Logic Flow:

**Step 1: Calculate dimensions (lines 588-590):**
```c
int num_digits = calculate_digits (buffer_num_lines (buf));
int num_width = show_line_numbers ? num_digits + 1 : 0;
int available_width = COLS - 2 - num_width;  // Screen width for text
```

**Step 2: Main rendering loop (lines 592-745):**
```c
int visual_row = 0;      // Current screen row being drawn
int logical_line = *scroll_row;  // Current logical line in buffer

while (visual_row < max_lines && logical_line < buffer_num_lines (buf))
```

#### Word Wrap ON Path (lines 614-690):
When `config->display.word_wrap == 1`:

```c
if (config->display.word_wrap)
{
    // Word wrap enabled: split across multiple visual rows
    while (pos < len && visual_row < max_lines)
    {
        // Show line number only on first visual row
        if (show_line_numbers && pos == ...)
            mvprintw (1 + visual_row, 1, "%*u ", num_digits, logical_line + 1);
        
        int segment_len = available_width;
        
        // Break at space if possible (word boundary)
        if (pos + segment_len < len)
        {
            int break_at = segment_len;
            for (int i = segment_len; i > 0; i--)
            {
                if (line[pos + i] == ' ')
                {
                    break_at = i;
                    break;
                }
            }
            segment_len = break_at;
            if (segment_len == 0)
                segment_len = available_width;  // Hard break if no space
        }
        
        // Print segment respecting selection highlighting
        int seg_end = pos + segment_len;
        
        // Before selection
        if (pos < sel_start && seg_end > pos) { ... }
        
        // Selection
        if (pos < sel_end && seg_end > pos) { ... }
        
        // After selection
        if (pos < seg_end) { ... }
        
        visual_row++;  // Move to next screen row
    }
}
```

**Key Features:**
- Breaks at word boundaries (spaces) when possible
- Falls back to hard break if no space found
- Shows line number only once per logical line
- Preserves selection highlighting across wrapped segments
- Maintains syntax highlighting throughout wrapped text

#### Word Wrap OFF Path (lines 691-742):
When `config->display.word_wrap == 0`:

```c
else
{
    // Word wrap disabled: truncate (original behavior)
    if (show_line_numbers)
        mvprintw (1 + visual_row, 1, "%*u ", num_digits, logical_line + 1);
    
    int x = 1 + num_width;
    
    // Print before selection
    if (pos < sel_start) { ... }
    
    // Print selection
    if (pos < sel_end) { ... }
    
    // Print after selection
    if (pos < len) { ... }
    
    visual_row++;  // Move to next screen row
}
```

**Behavior:**
- Truncates lines to available screen width
- Shows horizontal scroll position via `scroll_col`
- Only one visual row per logical line
- Selection highlighting still works on visible portion

---

## 4. WORD WRAP HELPER FUNCTIONS

### Calculate Visual Rows Per Line
**File:** `/home/larry/Documents/refgift/led/view.c`
**Function:** `visual_rows_for_line()` (lines 54-98)

```c
int
visual_rows_for_line (const char *line, int available_width)
{
    if (!line || available_width <= 0)
        return 1;
    
    int len = strlen (line);
    if (len == 0)
        return 1;
    
    int rows = 0;
    int pos = 0;
    
    while (pos < len)
    {
        int segment_len = available_width;
        
        // Break at space if possible
        if (pos + segment_len < len)
        {
            int break_at = segment_len;
            for (int i = segment_len; i > 0; i--)
            {
                if (line[pos + i] == ' ')
                {
                    break_at = i;
                    break;
                }
            }
            segment_len = break_at;
            if (segment_len == 0)
                segment_len = available_width;
        }
        else
        {
            segment_len = len - pos;
        }
        
        pos += segment_len;
        rows++;
    }
    
    return rows;
}
```

**Purpose:** Calculates how many screen rows a single logical line will occupy when word wrapping is enabled.

### Calculate Total Visual Lines in Buffer
**File:** `/home/larry/Documents/refgift/led/view.c`
**Function:** `calculate_total_visual_lines()` (lines 100-117)

```c
int
calculate_total_visual_lines (Buffer *buf, EditorConfig *config, int num_width)
{
    if (!config->display.word_wrap)
        return buffer_num_lines (buf);  // 1 visual row per logical line
    
    int available_width = COLS - 2 - num_width;
    if (available_width <= 0)
        available_width = 40;  // Fallback
    
    int total = 0;
    for (int i = 0; i < buffer_num_lines (buf); i++)
    {
        const char *line = buffer_get_line (buf, i);
        total += visual_rows_for_line (line, available_width);
    }
    return total;
}
```

**Purpose:** Calculates total number of visual rows needed to display entire buffer (accounting for word wrapping).

---

## 5. STATUS BAR UPDATES

### Status Bar Rendering
**File:** `/home/larry/Documents/refgift/led/view.c`
**Function:** `draw_update()` (lines 746-858)

### Status Components (lines 747-847):

1. **Temporary Message (lines 750-755):**
   - Shows operation feedback (e.g., "Auto-saved")
   - Displays for 5 seconds
   ```c
   if (ed && ed->status_message[0]
       && (time (NULL) - ed->status_message_time) < 5)
   {
       snprintf (message_prefix, COLS, "%s | ", ed->status_message);
   }
   ```

2. **Search/Replace Mode (lines 756-770):**
   ```c
   if (replace_step == 1)
       snprintf (status_line, COLS, "Replace search: %s", ...);
   else if (replace_step == 2)
       snprintf (status_line, COLS, "Replace with: %s", ...);
   else if (search_mode)
       snprintf (status_line, COLS, "Search: %s", ...);
   ```

3. **Standard Status (lines 771-847):**
   - Time display (optional, configurable)
   - Version display (optional)
   - Line and column position
   - Filename with modification indicator (*)
   - Two layout styles: Centered or Balanced

### Position String (lines 793-795):
```c
char pos_str[64];
snprintf (pos_str, 64, "Line %u/%u Col %u",
         cursor_line + 1, buffer_num_lines (buf), cursor_col + 1);
```

### Final Rendering (line 858):
```c
mvprintw (LINES - 1, 1, "%s", status_line);  // Draw at bottom row
```

---

## 6. CONFIG STRUCTURE

**File:** `/home/larry/Documents/refgift/led/config.h`

```c
typedef struct {
    int show_line_numbers;      // F2 toggles this
    int syntax_highlight;
    int word_wrap;              // F3 toggles this (0=OFF, 1=ON)
    int tab_width;
    int spaces_for_tab;
} DisplayConfig;
```

**Initialization (config.c line 83):**
```c
config->display.word_wrap = 0;  // Default: OFF
```

---

## 7. EDITOR STATE TRACKING

**File:** `/home/larry/Documents/refgift/led/editor.h`

```c
typedef struct {
    Buffer model;
    int scroll_row;             // First visible logical line
    int scroll_col;             // First visible column (when wrap OFF)
    int show_line_numbers;      // F2 toggle state
    int syntax_highlight;
    int cursor_line;            // Logical line position
    int cursor_col;             // Logical column position
    // ... selection, search, replace fields ...
    EditorConfig config;        // Contains display.word_wrap flag
    // ... auto-save fields ...
    char status_message[256];
    time_t status_message_time;
    int file_modified;
    int total_visual_lines;     // For word wrap (not currently used)
} Editor;
```

---

## 8. FLOW DIAGRAM: F3 PRESS → DISPLAY UPDATE

```
User presses F3
    ↓
main.c:80 → getch() receives KEY_F(3)
    ↓
main.c:83 → editor_handle_input(&ed, ch)
    ↓
editor.c:150-154 → Toggle: ed->config.display.word_wrap = !ed->config.display.word_wrap
    ↓
main.c:84 → editor_draw(stdscr, &ed)
    ↓
editor.c:130-139 → draw_update(..., &ed->config, ed)
    ↓
view.c:614 → Check config->display.word_wrap
    ├─ If 1 (ON):  Draw lines 614-690 (word wrap rendering)
    └─ If 0 (OFF): Draw lines 691-742 (truncate rendering)
    ↓
view.c:858 → Draw status bar with current line count
    ↓
Screen updates with new rendering
```

---

## 9. KEY FILES SUMMARY TABLE

| File | Key Functions/Structures | Purpose |
|------|--------------------------|---------|
| `editor.c` | `editor_handle_input()` | F3 key toggle, input dispatch |
| `view.c` | `draw_update()` | Main rendering with word wrap logic |
| `view.c` | `visual_rows_for_line()` | Calculate visual rows per logical line |
| `view.c` | `calculate_total_visual_lines()` | Calculate total visual rows in buffer |
| `model.c` | `buffer_num_lines()` | Get logical line count |
| `config.h` | `DisplayConfig` struct | Configuration flags including `word_wrap` |
| `config.c` | `set_default_config()` | Initialize `word_wrap` to 0 |
| `main.c` | Event loop (lines 78-85) | Read input, dispatch to handler, redraw |
| `editor.h` | `Editor` struct | Holds config and display state |

---

## 10. TESTING

**Test file:** `/home/larry/Documents/refgift/led/test_controller.c`

**Test function:** `test_word_wrap_toggle()` (lines 235-290)

```bash
# Run all tests
cd /home/larry/Documents/refgift/led
./led -t
```

**What it tests:**
- F3 toggle actually flips the flag
- Buffer remains unchanged when toggling
- Line count unchanged when toggling
- Toggle works multiple times

---

## 11. IMPORTANT NOTES

1. **Word Wrap Toggle Works, but Display Not Fully Implemented:**
   - The F3 key correctly toggles `config->display.word_wrap`
   - The `draw_update()` function HAS the logic to check the flag
   - Lines 614-690 contain word wrapping rendering code
   - However, the WORDWRAP_SPEC.md indicates this feature may need refinement

2. **Line Count Always Shows Logical Lines:**
   - Status bar always shows `Line X/Y` where Y is the logical line count
   - Does NOT change based on word wrap setting
   - Word wrapping only affects visual display, not model

3. **Selection Highlighting Works Across Wrapped Segments:**
   - The rendering code properly handles selection across word-wrapped lines
   - "Before selection", "Selection", "After selection" logic in lines 654-686

4. **Horizontal Scrolling (scroll_col) Only Used When Word Wrap OFF:**
   - Line 599: `int pos = (*scroll_col && !config->display.word_wrap) ? *scroll_col : 0;`
   - When word wrap is ON, horizontal scroll is ignored

5. **Syntax Highlighting Preserved Through Word Wrapping:**
   - The `print_highlighted()` function handles segments of lines
   - Colors applied per segment when line is wrapped

