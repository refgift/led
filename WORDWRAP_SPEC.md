# Word Wrap Feature Specification

## Current Status
**REMOVED** — Feature disabled to ensure reliable editing functionality.
Toggle (F3) and all word wrap code removed. Editor always uses truncate for long lines.

## Problem
- `config->display.word_wrap` exists and toggles via F3
- **But `view.c` never checks `config->display.word_wrap`**
- Long lines are truncated at screen boundary regardless of toggle state
- No test validates the feature

## Architecture

### Model Layer (model.c)
- **No change needed**
- Buffer stores logical lines unchanged
- `buffer_get_line()` always returns full line

### View Layer (view.c) — **NEEDS IMPLEMENTATION**
- Currently: Truncates lines to fit screen width
  ```c
  int max_print = (int)(COLS - 2 - num_width);  // Just cuts off
  if (print_len > max_print) print_len = max_print;
  ```

- Needed: When `config->display.word_wrap == 1`:
  - Split long logical lines into multiple visual rows
  - Track which visual row corresponds to which logical position
  - Handle selection/highlighting across wrapped segments
  - Maintain cursor position tracking through wrapped text

- When `config->display.word_wrap == 0`:
  - Keep current behavior (truncate + maybe show indicator like `→`)

### Controller Layer (editor.c)
- ✓ Toggle already works (F3)
- **Needs update**: Cursor movement (arrows, page up/down) must account for wrapped lines
  - Up arrow in wrapped text might move to visual row above (same logical line)
  - Or move to logical line above (current behavior)
  - **Decision needed**: UX for cursor in wrapped text?

## Test Case
✓ **Added to `test_controller.c`** — runs via `led -t`

Test: `test_word_wrap_toggle()`
- Creates a long line (140+ chars)
- Toggles `config->display.word_wrap` OFF and ON
- Verifies buffer is **unchanged** in all cases
- Confirms toggle flag actually flips

**Run tests:**
```bash
cd ~/Documents/refgift/led
./led -t
```

All word wrap assertions pass ✓ (model layer is correct)

## Implementation Checklist
- [x] Model: Verify buffer unchanged when toggle changes (test passes ✓)
- [x] Controller: Toggle works via F3 (already implemented)
- [ ] **VIEW: Implement actual word wrapping display logic**
  - [ ] Check `config->display.word_wrap` in `draw_update()` (~line 530)
  - [ ] When enabled: wrap long lines to multiple screen rows
  - [ ] When disabled: truncate long lines (current behavior, maybe add `→` indicator)
  - [ ] Track visual row to logical line mapping for cursor navigation
  - [ ] Update selection highlighting to work across wrapped segments
- [ ] Cursor movement: Define behavior in wrapped text (up/down arrow keys)
- [ ] Update README.md to document F3 (word wrap toggle)

## Related Code Sections
- **Toggle**: `editor.c` line ~150 (`else if (ch == KEY_F(3))`)
- **Rendering**: `view.c` function `draw_update()` starting at line 496
- **Config**: `config.h` struct `DisplayConfig` has `word_wrap` field
- **Highlight**: `view.c` function `print_highlighted()`

## Questions for Larry
1. **Cursor movement**: When a line is wrapped, should arrow keys move:
   - By visual row (Up/Down move within the wrapped line)?
   - By logical line (Up/Down skip the whole wrapped line)?
   
2. **Visual indicator**: Show `→` at line boundary when truncated (wrap OFF)?

3. **Priority**: Is this high priority for next iteration, or lower?
