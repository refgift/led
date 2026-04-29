# LED Editor - Codebase Index

This index provides quick navigation to understand how the LED text editor implements word wrap toggling, line counting, and view rendering.

## Documentation Files

Start here based on what you need:

1. **QUICK_REFERENCE.md** (3.4 KB)
   - Best for: "Where is [feature] implemented?"
   - Contains: File paths, line numbers, function names
   - Use when: You need a quick lookup table

2. **SEARCH_RESULTS.md** (14 KB)
   - Best for: "How does [feature] work in detail?"
   - Contains: Full code snippets, explanations, flow diagrams
   - Use when: You need to understand the complete implementation

3. **ARCHITECTURE.md** (21 KB)
   - Best for: "What's the big picture?"
   - Contains: ASCII flow diagrams, system architecture, data structures
   - Use when: You want to understand relationships between components

4. **WORDWRAP_SPEC.md** (existing file)
   - Best for: "What needs to be done?"
   - Contains: Feature specification, known issues, open questions

---

## Feature Quick Links

### F3 Key (Word Wrap Toggle)
- **Handler:** `editor.c:150-154` → `editor_handle_input()`
- **Toggle code:** `ed->config.display.word_wrap = !ed->config.display.word_wrap`
- **Event loop:** `main.c:78-85`
- **Full explanation:** SEARCH_RESULTS.md section 1, ARCHITECTURE.md "F3 Key Handling Flow"

### Line Count Display
- **Storage:** `model.h:8-12` → `Buffer.num_lines`
- **Getter:** `model.c:282-286` → `buffer_num_lines()`
- **Status bar:** `view.c:793-795` displays "Line X/Y Col Z"
- **Full explanation:** SEARCH_RESULTS.md section 2, ARCHITECTURE.md "Line Count Calculation"

### Word Wrap Rendering
- **Main function:** `view.c:558-884` → `draw_update()`
- **Decision point:** `view.c:614`
- **Wrap ON:** `view.c:614-690` (split lines across multiple visual rows)
- **Wrap OFF:** `view.c:691-742` (truncate lines)
- **Full explanation:** SEARCH_RESULTS.md section 3, ARCHITECTURE.md "View Rendering Pipeline"

### Status Bar
- **Location:** `view.c:746-858` (within `draw_update()`)
- **Render:** `view.c:858` → `mvprintw(LINES-1, 1, status_line)`
- **Components:** Time, version, filename, position, modification indicator
- **Full explanation:** SEARCH_RESULTS.md section 5

### Configuration
- **Config struct:** `config.h:32-38` → `DisplayConfig`
- **Initialization:** `config.c:83` → `word_wrap = 0` (default)
- **Access in editor:** `ed->config.display.word_wrap`
- **Full explanation:** SEARCH_RESULTS.md section 6

---

## Code Files Summary

### Core Files

| File | Purpose | Key Functions |
|------|---------|----------------|
| `main.c` | Entry point, event loop | Event loop (lines 78-85) |
| `editor.c` | Editor state management | `editor_handle_input()` (F3 handler) |
| `editor.h` | Editor structure definition | `Editor` struct |
| `view.c` | Rendering engine | `draw_update()`, `visual_rows_for_line()` |
| `view.h` | View declarations | Function prototypes |
| `model.c` | Text buffer implementation | `buffer_num_lines()` |
| `model.h` | Buffer structure definition | `Buffer` struct |
| `config.c` | Configuration loading | `set_default_config()` |
| `config.h` | Configuration structures | `DisplayConfig` struct |

### Test Files

| File | Purpose |
|------|---------|
| `test_controller.c` | Controller and toggle tests |
| `test_view.c` | View rendering tests |
| `test_symbols.txt` | Test data |

---

## Typical Workflows

### Workflow 1: Adding a Status Bar Feature
1. Check current status bar structure: `view.c:746-858`
2. See how other components display: `view.c:774-847`
3. Add field to status line building
4. Call `mvprintw()` at `view.c:858`

### Workflow 2: Understanding Word Wrap
1. Start: ARCHITECTURE.md "Word Wrap Decision Tree"
2. Read: `view.c:614-690` (wrap ON path)
3. Compare: `view.c:691-742` (wrap OFF path)
4. Helper functions: `visual_rows_for_line()` and `calculate_total_visual_lines()`

### Workflow 3: Modifying Line Count Behavior
1. Check storage: `model.c:282-286` → `buffer_num_lines()`
2. Check display: `view.c:793-795` → Uses `buffer_num_lines(buf)`
3. Note: Line count is ALWAYS logical (never affected by word wrap)
4. If you need visual line count: Use `calculate_total_visual_lines()` instead

### Workflow 4: Adding New Toggle Keys
1. Find existing F2 and F3 handlers in `editor.c:145-154`
2. Add new `else if (ch == KEY_F(4))` clause
3. Toggle your new feature flag
4. Re-render will use new flag in `draw_update()`

---

## Architecture Overview

**Model-View-Controller Pattern:**
```
Model (model.c)
├─ Buffer structure with logical lines
└─ Never modified by display settings

Controller (editor.c)
├─ Handles input (F3 key)
├─ Toggles config flags
└─ No model manipulation for display settings

View (view.c)
├─ Reads config flags
├─ Renders display based on flags
├─ Status bar shows current state
└─ User sees immediate changes
```

**Event Loop:**
```
main.c → getch() → editor_handle_input() → editor_draw() → view rendering
```

**Configuration Flow:**
```
config.c (load defaults) → editor.h (Editor struct) → config flags → view.c (check flags)
```

---

## Testing

Run tests:
```bash
cd /home/larry/Documents/refgift/led
./led -t
```

Key test: `test_word_wrap_toggle()` in `test_controller.c:235-290`
- Verifies F3 toggle works
- Confirms buffer unchanged by toggle
- Ensures line count remains constant

---

## Function Reference

### Most Important Functions

| Function | File | Lines | Purpose |
|----------|------|-------|---------|
| `main()` | main.c | 12-91 | Entry point, event loop setup |
| `editor_handle_input()` | editor.c | 142-238 | Keyboard input handler (includes F3) |
| `editor_draw()` | editor.c | 130-139 | Trigger render update |
| `draw_update()` | view.c | 558-884 | Main rendering function |
| `visual_rows_for_line()` | view.c | 54-98 | Calculate rows needed for line |
| `calculate_total_visual_lines()` | view.c | 100-117 | Calculate total visual rows |
| `buffer_num_lines()` | model.c | 282-286 | Get logical line count |
| `load_editor_config()` | config.c | 91-172 | Load configuration |
| `set_default_config()` | config.c | 44-89 | Initialize default config |

---

## Key Constants & Macros

From ncurses:
- `KEY_F(n)` - Function key n (F1-F12)
- `getch()` - Read keyboard input
- `mvprintw()` - Move and print
- `COLS` - Terminal width
- `LINES` - Terminal height
- `COLOR_PAIR(n)` - Select color pair

From project:
- `VERSION` - Current version (config.h)
- `INITIAL_LINES_CAPACITY` - Buffer initial size (model.h)
- `MAX_FILE_SIZE` - Max file size (model.c)

---

## Common Search Patterns

Looking for... | Where to look
---|---
"How does word wrap toggle?" | QUICK_REFERENCE.md → F3 Key Binding
"How do I display something on the status bar?" | view.c:746-858 (draw_update status bar section)
"Where's the line count stored?" | model.h:8-12 (Buffer struct)
"How are lines wrapped visually?" | view.c:614-690 (wrap path) + ARCHITECTURE.md diagrams
"What's the overall architecture?" | ARCHITECTURE.md (full diagrams and flows)
"Where's the event loop?" | main.c:78-85
"How to add a new feature?" | editor.h (add state), editor.c (handle input), view.c (render)

---

## Important Notes

1. **Line count is logical, not visual**
   - Status bar always shows buffer line count
   - Word wrapping doesn't change the number shown
   - Word wrapping only affects display layout

2. **Model is never modified by display settings**
   - Buffer content unchanged by F3 toggle
   - Only view rendering changes
   - This is MVC pattern separation

3. **Horizontal scrolling disabled when word wrap ON**
   - Line 599 in view.c checks this
   - When wrapped, scroll_col is ignored
   - Makes sense: can't scroll a wrapped line

4. **Selection highlighting works across wrapped segments**
   - See view.c:654-686 (before/during/after selection)
   - Colors applied to each segment independently
   - Provides continuous selection feedback

5. **Syntax highlighting preserved through wrapping**
   - print_highlighted() called per segment
   - Colors maintained as line is broken up
   - Seamless highlighting experience

---

## Next Steps

- For implementation details: → SEARCH_RESULTS.md
- For system overview: → ARCHITECTURE.md
- For quick lookups: → QUICK_REFERENCE.md
- For feature spec: → WORDWRAP_SPEC.md
- To run code: `./led filename` or `./led -t` for tests

