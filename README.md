# Larry's Editor
- editor.h 39.50  Trust begins to bloom / Routine becomes second nature / Anchored in the storm
- led - Larry's Editor for Linux/Unix Terminal (1.0.8)
- Dedicated to Neal Stephenson's vision of paper-like data safety.
- Author: Larry Bruce Daniel. Assistance from Grok (xAI): grok-code-fast-1 on earlier versions, Grok 4.7 on 1.0.7.

## What Works
- **Data safety**: undo/redo (1M-change cap, oldest dropped first; one step per key press, cut, or paste), auto-save with versioned backups, recovery file written when the terminal closes (SIGHUP/SIGTERM). A file that did not load whole (too large, binary, unreadable, not a regular file, or with cut lines) is never written back. Saving through a symlink keeps the link. Rejects files over `max_file_size_mb` (default 10MB) or with null bytes, and files whose line cache would pass `memory_limit_mb`. Cuts lines at `max_line_length`.
- **Editing**: insert, newline, backspace/delete, tabs (spaces or `\t`), word wrap (`F3`), select all (`Ctrl+A`) then copy / cut / Backspace act on the whole buffer, clipboard (`Ctrl+C/X/V`, multi-line, undoable), regex search (`Ctrl+/`) and replace (`Ctrl+R`), syntax highlighting (C/C++ nesting, pre-parsed, per-line cache).
- **Search chord**: `Ctrl+/` opens search. Byte 31 still works. foot and xterm modifyOtherKeys (`ESC [27;5;47~`) and kitty CSI-u (`ESC [47;5u`) are the same chord, so that sequence is not typed into the buffer.
- **Display**: `F2` line numbers, `F3` word wrap, `F4` border toggle (see below), status bar (version/time/position/key-meter `show_key_meter`), Unicode/Cyrillic (arrows, Backspace, Delete, and undo work on whole characters).
- **Incremental rendering**: cursor moves repaint only status line, single-line edits repaint only that row (75x less output); `view_decide()` + nesting cache.
- **Border handling**: text lives in its own pane inside the screen: rows-2 × cols-2 at (1,1) with the border on, rows-1 × cols at (0,0) with it off; border only on `FULL` repaint. Paste-time `border_filter_dup()` strips `| + - │ ─ ┌ ┐ └ ┘ ├ ┤ ┬ ┴ ┼ ┃ ━` from the ends of each line and drops pure `┌──┐` lines — replaces `tr -d`.
- **Config**: two files in `~/.config/led/`, written on first run with every default as a comment: `led.conf` (general settings) and `colorization.conf` (colors by name, bright variant, number or `DEFAULT`; `reserved_words`, `paired_keywords`, `syntax_extensions`, `show_key_meter`, `show_border`). `--prefix DIR` keeps them in `DIR/led/` where `~/.config` is not the place. Env `LED_NO_BORDER=1` / `LED_SHOW_BORDER`.
- **C/C++ editing**: `file:line:col` and `+N` open on the diagnostic, `Ctrl+G` jumps there, `Ctrl+]` matches braces, word motions, `Ctrl+K` toggles `//`, Enter keeps indent, Shift+Tab outdents. Source files show line numbers.
- **Tests**: 363 tests (buffer, undo, clipboard, autosave, view, search wrap, search chord, indent, braces, save, data safety, config files, screen contents, controller actions, colours). `./led -t` on your machine.
- **Paths**: a file with `..` in its path (`../src/file.c`) opens. It used to fail.
- **First frame, resize, `F4`**: these draw through the same painter as a key press. Before, they showed no status line, dropped word wrap, and cut a line with non-ASCII characters short (sometimes ending in `??`) until the next key.

## What Fails
- **Large files**: the whole file is still loaded into memory. There is no lazy load or mmap. led stops when the file exceeds `max_file_size_mb`, or when the next line would push the nesting cache past `memory_limit_mb`. The buffer stays empty and the status line says `File too large` or `Too many lines`. Raise those keys to accept the cost, or split the file first.
- **Long lines**: a line past `max_line_length` (default 10000) is cut on load. The status line says `Long line truncated`. Cut lines skip highlighting and use basic rendering. A file with cut lines opens read-only (`Ctrl+S` and autosave refuse). Raise `max_line_length` to edit it.
- **Limits**: `max_file_size_mb` (1–100, default 10), `memory_limit_mb` (1–200, default 50), and `max_line_length` (80–100000, default 10000) are keys in `colorization.conf`. A value outside its range keeps the default.
- **Language support**: highlighting and parsing are built for C/C++; other languages may be incomplete. Turn highlighting off in config if a file fights you. Strings, character literals and comments are left plain: brackets and keywords inside them are not coloured and do not nest. A `paired_keywords` pair is used only when both words, or neither, are in `reserved_words`, so the default `if-then` is ignored for C and `if` is coloured as a reserved word.
- **Error handling**: not fully standardized across modules.
- **No per-buffer undo persistence** across sessions.
- **Replace** (`Ctrl+R`) cannot be undone and clears the undo history. The status line says so.
- **No fuzzing harness** yet. Empty-file, size, null-byte, truncation, and memory-ceiling loads are tested.
- **Input sanitization**: `is_filename_safe` rejects only empty names, names over 255 bytes, and control characters. There is no directory-traversal check; `..` is allowed.
- **Terminal**: `xterm` block-select still copies screen cells — use `F4` off or filter; no OSC 52 system clipboard.
- **Other languages**: a file such as Python is not highlighted by default. Add its extension to `syntax_extensions` in `colorization.conf` (for example `syntax_extensions=.c,.h,.cpp,.py`) and it is highlighted with the C rules.
- **Garbled display**: if the screen looks corrupted, press `F4` twice (border on, then off again). `F4` repaints the whole screen. `F2` is not a repaint key. A file whose extension is not in `syntax_extensions` should need no workaround; if one does, that is a bug to report.

Bugs: size and type (`file`, `head` sample), or a `gdb ./led` backtrace, at https://github.com/refgift/led/issues

## Design: model / view / controller / front end
The view draws through three functions with one signature, declared in `frontend/frontend.h`:

- `mark(x, y, c)` — the cell holds character `c` (its Unicode number).
- `show(x, y, c)` — the cell is seen with quality `c` (normal, selection, reserved word, ...) and the cursor rests there.
- `hide(x, y, c)` — the cell is blanked with `c`, a space.

For each cell that changed: `mark` it and `show` it, or `hide` it; finish with a `show` where the caret is. `view/screen.c` keeps a grid of cells and sends only the ones that differ.

A front end supplies those three functions plus start, stop, size and read-key. `frontend_curses.c` is the only file that includes ncurses. `frontend_memory.c` is a grid the tests read, so what reaches the screen is tested without a terminal. `frontend_ansi.c` is a stub with notes on how to finish it.

The controller is three functions with one signature too, declared in `controller/controller.h`:

- `symbol(action, value)` — one action, one key press: `symbol(S_INSERT, 'a')`, `symbol(S_LEFT, 3)`.
- `macro(action, value)` — an action made of several symbols, and one undo step: `macro(M_ENTER, 1)` is a newline plus the indent.
- `token(action, value)` — keys collected into a unit before anything happens (search, replace, go to line, `Esc` then a key): `T_BEGIN`, then `T_CHAR` per character, then `T_DONE` or `T_CANCEL`.

One table in `controller.c` says which call each key makes. A test, or a recorded macro, is a list of those calls with no keys involved.

## Dependencies
C compiler, GNU `make` (`gmake` on BSD and other Unix), POSIX `regex.h` (in libc), and `libncursesw` for the default front end.

## Build / Install / Test
```
make              # builds ./led with the curses front end
make UI=ansi      # no ncurses; the ANSI front end is not written yet (make clean first)
make install      # installs to /usr/local/bin
make doc          # installs led.1
./led file.c
./led +142 file.c       # open at a line (compiler output)
./led file.c:142:8      # file:line or file:line:col
./led --prefix DIR file.c   # config files in DIR/led, not ~/.config/led
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
| `Ctrl+/` / `Ctrl+R` | Search / Replace (regex, wraps, 100-char limit). `Ctrl+/` is byte 31 or the terminal's modified-key sequence |
| `F2` | Line numbers |
| `F3` | Word wrap |
| `F4` | Border on/off (subwindow inset vs full-width; pasted `│` stripped) |

## Configuration
Two files in `~/.config/led/`, both `key=value` with `#` comments: `led.conf` for general settings and `colorization.conf` for colours and syntax words. led writes each one on first run if it is missing, with every default shown as a comment; remove the `#` from a line to change it. An existing file is never rewritten.

Either file may hold any key, so a `colorization.conf` from before `led.conf` existed keeps working. `led.conf` is read last and wins when both set a key.

`led --prefix DIR file` uses `DIR/led/` in place of `~/.config/led/`, for systems that keep settings somewhere else.

**Colors** (`BLACK,RED,GREEN,YELLOW,BLUE,MAGENTA,CYAN,WHITE`; a bright variant of each as `BRIGHT_BLUE` or `LIGHT_BLUE`; `GRAY`; `DEFAULT` for the terminal's own colour; or a number `0`–`255`; any letter case): `normal_fg/bg`, `selection_fg/bg`, `semicolon_fg/bg`, `meta_level1..4_fg/bg`, `reserved_words_fg/bg`.

| Number | Dark (plain name) | Number | Light |
|---|---|---|---|
| 0 | `BLACK` | 8 | `GRAY` (or `BRIGHT_BLACK`) |
| 1 | `RED` | 9 | `LIGHT_RED` |
| 2 | `GREEN` | 10 | `LIGHT_GREEN` |
| 3 | `YELLOW` | 11 | `LIGHT_YELLOW` |
| 4 | `BLUE` | 12 | `LIGHT_BLUE` |
| 5 | `MAGENTA` | 13 | `LIGHT_MAGENTA` |
| 6 | `CYAN` | 14 | `LIGHT_CYAN` |
| 7 | `WHITE` | 15 | `LIGHT_WHITE` |

`BRIGHT_` works wherever `LIGHT_` does. Defaults: text `WHITE` on `BLACK`, selection `BLACK` on `WHITE`, semicolon and reserved words `RED`, nesting levels 1–4 `BLUE`, `CYAN`, `GREEN`, `YELLOW`.

The eight plain names are the dark set. Bright variants are colours 8–15 on a terminal with 16 or more colours; on an 8-colour terminal led shows the plain colour in bold instead (see `curs_color(3x)`). A number above what the terminal has falls back to white.

Example, light colours on the terminal's own background:
```
normal_bg=DEFAULT
reserved_words_fg=LIGHT_RED
meta_level1_fg=LIGHT_BLUE
meta_level2_fg=LIGHT_CYAN
meta_level3_fg=LIGHT_GREEN
meta_level4_fg=LIGHT_YELLOW
semicolon_fg=208
```
Give every `_bg` key the same value, or the background changes from word to word.

**Display/Border**: `show_border=0` (default off, `1` shows box). Toggled by `F4`.

**Other**: `syntax_extensions=.c,.h,.C,.H,.cpp,.cc,.cxx,.hpp,.hh,.hxx`, `reserved_words=...` (C and C++ keywords), `paired_keywords=if-then,begin-end,(,)`, `tab_width=8`, `spaces_for_tab=0`, `show_key_meter=1`, `show_line_numbers=0`.

**Limits**: `max_file_size_mb=10` (1–100), `memory_limit_mb=50` (1–200), `max_line_length=10000` (80–100000). A file over the size or memory ceiling is not opened. A longer line is cut and the status line says `Long line truncated`. A null byte is refused with `Binary file`.

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
