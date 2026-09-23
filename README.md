# led - Larry's Editor for Linux/Unix Terminal (1.0.7)

Dedicated to Neal Stephenson's vision of paper-like data safety.

Author: Larry Bruce Daniel. Assistance from Grok (xAI): grok-code-fast-1 on earlier versions, Grok 4.7 on 1.0.7.

## What Works
- **Data safety**: unlimited undo/redo (10k cap), auto-save with versioned backups, crash recovery. Rejects files >10MB or with null bytes; validates line length.
- **Editing**: insert, newline, backspace/delete, tabs (spaces or `\t`), word wrap (`F3`), selection, clipboard (`Ctrl+A/C/X/V`), regex search (`Ctrl+/`) and replace (`Ctrl+R`), syntax highlighting (C/C++ nesting, pre-parsed, per-line cache).
- **Display**: `F2` line numbers, `F3` word wrap, `F4` border toggle (see below), status bar (version/time/position/key-meter `show_key_meter`), Unicode/Cyrillic.
- **Incremental rendering**: cursor moves repaint only status line, single-line edits repaint only that row (75x less output); `view_decide()` + nesting cache.
- **Border handling**: text in `derwin(stdscr, LINES-2,COLS-2,1,1)` (border on) or `derwin(stdscr, LINES-1,COLS,0,0)` (border off); border only on `FULL` repaint. Paste-time `border_filter_dup()` strips `| + - │ ─ ┌ ┐ └ ┘ ├ ┤ ┬ ┴ ┼` per line and drops pure `┌──┐` lines — replaces `tr -d`.
- **Config**: `~/.config/led/colorization.conf` (colors, `reserved_words`, `paired_keywords`, `syntax_extensions`, `show_key_meter`, `show_border`), env `LED_NO_BORDER=1` / `LED_SHOW_BORDER`.
- **C/C++ editing**: `file:line:col` and `+N` open on the diagnostic, `Ctrl+G` jumps there, `Ctrl+]` matches braces, word motions, `Ctrl+K` toggles `//`, Enter keeps indent, Shift+Tab outdents. Source files show line numbers.
- **Tests**: 193 tests (buffer, undo, clipboard, autosave, view, search wrap, indent, braces, save). `./led -t` on your machine.

## What Fails
- **Large files**: whole file loaded into memory, no lazy/mmap. Past about 30000 lines it hogs the system and you may have to kill the process. Split huge files first.
- **Long lines**: truncates past 10k characters; those lines skip highlighting and use basic rendering.
- **Hardcoded limits**: file size and line length are not configurable yet.
- **Language support**: highlighting and parsing are built for C/C++; other languages may be incomplete. Turn highlighting off in config if a file fights you.
- **Error handling**: not fully standardized across modules.
- **No per-buffer undo persistence** across sessions.
- **No fuzzing / empty-file edge coverage** yet.
- **Input sanitization**: no directory-traversal check beyond `is_filename_safe`.
- **Terminal**: `xterm` block-select still copies screen cells — use `F4` off or filter; no OSC 52 system clipboard.

Bugs: size and type (`file`, `head` sample), or a `gdb ./led` backtrace, at https://github.com/refgift/led/issues

## Dependencies
C compiler, `make` (`gmake` on Unix), `libncursesw`, `glibc regex`.

## Build / Install / Test
```
make              # builds ./led
make install      # installs to /usr/local/bin
make doc          # installs led.1
./led file.c
./led +142 file.c       # open at a line (compiler output)
./led file.c:142:8      # file:line or file:line:col
./led -t          # test mode (stderr, no curses) — also LED_TEST=1 ./led
man led           # after make doc
```

## Keybindings
| Keys | Action |
|---|---|
| Arrows, Home/End (×2 top/bottom), PgUp/PgDn | Navigation |
| `Ctrl+Left` / `Ctrl+Right`, or `Esc` then `b` / `f` | Previous / next word (`_` counts as part of a word) |
| `Ctrl+]` | Jump to the matching `()` `[]` `{}` (skips strings and comments) |
| `Ctrl+G` | Go to line, or `line:col` |
| Printable, Enter, Backspace/Delete, Tab, Shift+Tab | Insert / delete / outdent |
| Enter at end of line | Copy indent; one extra level after `{` or `(` |
| `Ctrl+K` | Toggle a `//` comment on this line |
| `Ctrl+W` | Delete the previous word |
| `Ctrl+Z` / `Ctrl+Y` | Undo / Redo |
| `Ctrl+S` / `Ctrl+Q` | Save / Quit (Ctrl+Q again if the buffer is modified) |
| `Ctrl+A` / `Ctrl+C` / `Ctrl+X` / `Ctrl+V` | Select all / Copy / Cut / Paste (paste is border-filtered) |
| `Ctrl+/` / `Ctrl+R` | Search / Replace (regex, wraps, 100-char limit) |
| `F2` | Line numbers |
| `F3` | Word wrap |
| `F4` | Border on/off (subwindow inset vs full-width; pasted `│` stripped) |

## Configuration
`~/.config/led/colorization.conf` — `key=value`, `#` comments, created on first run.

**Colors** (`BLACK,RED,GREEN,YELLOW,BLUE,MAGENTA,CYAN,WHITE`): `normal_fg/bg`, `selection_fg/bg`, `semicolon_fg/bg`, `meta_level1..4_fg/bg`, `reserved_words_fg/bg`.

**Display/Border**: `show_border=0` (default off, `1` shows box). Toggled by `F4`.

**Other**: `syntax_extensions=.c,.h,.C,.H,.cpp,.cc,.cxx,.hpp,.hh,.hxx`, `reserved_words=...` (C and C++ keywords), `paired_keywords=if-then,begin-end,(,)`, `tab_width=8`, `spaces_for_tab=0`, `show_key_meter=1`, `show_line_numbers=0`.

Source files (those extensions) show line numbers unless `show_line_numbers` is set in the config. `Ctrl+S` replaces the file by rename and keeps its permission bits. Search wraps to the top and says so.

Example:
```
normal_fg=WHITE
normal_bg=BLACK
selection_fg=CYAN
selection_bg=BLACK
show_key_meter=1
show_border=0
syntax_extensions=.c,.h,.cpp
```
