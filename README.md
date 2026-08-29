# led - Larry's Editor for Linux/Unix Terminal (1.0.6)
> See WARNINGS.md for known limitations and safety notes.

Dedicated to Neal Stephenson's vision of paper-like data safety.

## What Works
- **Data safety**: unlimited undo/redo (10k cap), auto-save with versioned backups, crash recovery, file size/line-length validation.
- **Editing**: insert, newline, backspace/delete, tabs (spaces or `\t`), word wrap (`F3`), selection, clipboard (`Ctrl+A/C/X/V`), regex search (`Ctrl+/`) and replace (`Ctrl+R`), syntax highlighting (C/C++ nesting, pre-parsed, per-line cache).
- **Display**: `F2` line numbers, `F3` word wrap, `F4` border toggle (see below), status bar (version/time/position/key-meter `show_key_meter`), Unicode/Cyrillic.
- **Incremental rendering**: cursor moves repaint only status line, single-line edits repaint only that row (75x less output); `view_decide()` + nesting cache.
- **Border handling**: text in `derwin(stdscr, LINES-2,COLS-2,1,1)` (border on) or `derwin(stdscr, LINES-1,COLS,0,0)` (border off); border only on `FULL` repaint. Paste-time `border_filter_dup()` strips `| + - │ ─ ┌ ┐ └ ┘ ├ ┤ ┬ ┴ ┼` per line and drops pure `┌──┐` lines — replaces `tr -d`.
- **Config**: `~/.config/led/colorization.conf` (colors, `reserved_words`, `paired_keywords`, `syntax_extensions`, `show_key_meter`, `show_border`), env `LED_NO_BORDER=1` / `LED_SHOW_BORDER`.
- **Tests**: 133 tests (buffer, undo, clipboard, autosave, view truncate, change tracking, `view_decide`).

## What Fails
- **Large files**: whole file loaded into memory, no lazy/mmap, truncates lines >10k.
- **Hardcoded limits**: file size, line length not yet configurable.
- **Error handling**: not fully standardized across modules.
- **No per-buffer undo persistence** across sessions.
- **No fuzzing / empty-file edge coverage** yet.
- **Input sanitization**: no directory-traversal check beyond `is_filename_safe`.
- **Terminal**: `xterm` block-select still copies screen cells — use `F4` off or filter; no OSC 52 system clipboard.

## Dependencies
C compiler, `make` (`gmake` on Unix), `libncursesw`, `glibc regex`.

## Build / Install / Test
```
make              # builds ./led
make install      # installs to /usr/local/bin
make doc          # installs led.1
./led file.c
./led -t          # test mode (stderr, no curses) — also LED_TEST=1 ./led
man led           # after make doc
```

## Keybindings
| Keys | Action |
|---|---|
| Arrows, Home/End (×2 top/bottom), PgUp/PgDn | Navigation |
| Printable, Enter, Backspace/Delete, Tab | Insert / delete |
| `Ctrl+Z` / `Ctrl+Y` | Undo / Redo |
| `Ctrl+S` / `Ctrl+Q` | Save / Quit |
| `Ctrl+A` / `Ctrl+C` / `Ctrl+X` / `Ctrl+V` | Select all / Copy / Cut / Paste (paste is border-filtered) |
| `Ctrl+/` / `Ctrl+R` | Search / Replace (regex, 100-char limit) |
| `F2` | Line numbers |
| `F3` | Word wrap |
| `F4` | Border on/off (subwindow inset vs full-width; pasted `│` stripped) |

## Configuration
`~/.config/led/colorization.conf` — `key=value`, `#` comments, created on first run.

**Colors** (`BLACK,RED,GREEN,YELLOW,BLUE,MAGENTA,CYAN,WHITE`): `normal_fg/bg`, `selection_fg/bg`, `semicolon_fg/bg`, `meta_level1..4_fg/bg`, `reserved_words_fg/bg`.

**Display/Border**: `show_border=1` (default on, `0` hides box). Toggled by `F4`.

**Other**: `syntax_extensions=.c,.h,.cpp`, `reserved_words=...`, `paired_keywords=if-then,begin-end,(,)`, `tab_width=8`, `spaces_for_tab=0`, `show_key_meter=1`.

Example:
```
normal_fg=WHITE
normal_bg=BLACK
selection_fg=CYAN
selection_bg=BLACK
show_key_meter=1
show_border=1
syntax_extensions=.c,.h,.cpp
```
