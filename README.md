# Larry's Editor
- editor.h 39.50  Trust begins to bloom / Routine becomes second nature / Anchored in the storm
- led - Larry's Editor for Linux/Unix Terminal (1.0.8)
- Dedicated to Neal Stephenson's vision of paper-like data safety.
- Author: Larry Bruce Daniel. Assistance from Grok (xAI): grok-code-fast-1 on earlier versions, Grok 4.7 on 1.0.7.

## What Works
- **Data safety**: undo/redo (1M-change cap, oldest dropped first; one step per key press, cut, or paste), auto-save with versioned backups, recovery file written when the terminal closes (SIGHUP/SIGTERM). A file that did not load whole (too large, binary, unreadable, not a regular file, or with cut lines) is never written back. Saving through a symlink keeps the link. Rejects files over `max_file_size_mb` (default 10MB) or with null bytes, and files whose line cache would pass `memory_limit_mb`. Cuts lines at `max_line_length`.
- **File names**: any path the system allows, `..` included: led opens what it is given, with your permissions. Refused, with `File name not usable` on the status line: an empty name, a name with a control character (it would corrupt the status line), and a path within 32 bytes of the system limit (led writes `.recovery`, `.bak.N` and a temporary file beside it).
- **Undo across sessions**: the undo and redo history is written at every save and read back when the file is next opened, so `Ctrl+Z` still works after quitting. It is kept in `/tmp/led-UID/` (or under `$TMPDIR`), a directory of your own with mode 0700, which the system clears, so old histories do not pile up. It is used only if the file is byte for byte what it was at that save; a file changed by anything else gets no history. The newest 100,000 changes are kept. `persistent_undo=0` in `led.conf` turns it off.
- **Editing**: insert, newline, backspace/delete, tabs (spaces or `\t`), word wrap (`F3`), select all (`Ctrl+A`) then copy / cut / Backspace act on the whole buffer, clipboard (`Ctrl+C/X/V`, multi-line, undoable), regex search (`Ctrl+/`) and replace-all (`Ctrl+R`, one undo step), syntax highlighting (C/C++ nesting, pre-parsed, per-line cache). Lines of any length: a 5 MB single-line file opens, edits and saves.
- **System clipboard**: `Ctrl+C` and `Ctrl+X` hand the text to the terminal with OSC 52, which works over ssh. Text pasted by the terminal (bracketed paste) is inserted as text, so indented code does not staircase and control characters in it run no command. `system_clipboard=0` in `led.conf` turns the outgoing half off.
- **Search chord**: `Ctrl+/` opens search. Byte 31 still works. foot and xterm modifyOtherKeys (`ESC [27;5;47~`) and kitty CSI-u (`ESC [47;5u`) are the same chord, so that sequence is not typed into the buffer.
- **Display**: `F2` line numbers, `F3` word wrap, `F4` border toggle (see below), status bar (version/time/position/key-meter `show_key_meter`), Unicode/Cyrillic (arrows, Backspace, Delete, and undo work on whole characters).
- **Incremental rendering**: cursor moves repaint only status line, single-line edits repaint only that row (75x less output); `view_decide()` + nesting cache.
- **Border handling**: text lives in its own pane inside the screen: rows-2 × cols-2 at (1,1) with the border on, rows-1 × cols at (0,0) with it off; border only on `FULL` repaint. Paste-time `border_filter_dup()` strips `| + - │ ─ ┌ ┐ └ ┘ ├ ┤ ┬ ┴ ┼ ┃ ━` from the ends of each line and drops pure `┌──┐` lines — replaces `tr -d`.
- **Config**: two files in `~/.config/led/`, written on first run with every default as a comment: `led.conf` (general settings) and `colorization.conf` (the eight curses colours by name, the ANSI extended colours by `LIGHT_` name or number, or `DEFAULT`; `reserved_words`, `paired_keywords`, `syntax_extensions`, `show_key_meter`, `show_border`). `--prefix DIR` keeps them in `DIR/led/` where `~/.config` is not the place. Env `LED_NO_BORDER=1` / `LED_SHOW_BORDER`.
- **C/C++ editing**: `file:line:col` and `+N` open on the diagnostic, `Ctrl+G` jumps there, `Ctrl+]` matches braces, word motions, `Ctrl+K` toggles `//`, Enter keeps indent, Shift+Tab outdents. Source files show line numbers.
- **Tests**: 409 tests (buffer, undo, clipboard, autosave, view, search wrap, search chord, indent, braces, save, data safety, config files, screen contents, controller actions, system clipboard, undo history, colours, large files). `./led -t` on your machine.
- **Paths**: a file with `..` in its path (`../src/file.c`) opens. It used to fail.
- **No terminal scrolling**: when several rows change at once, led has curses write those rows out whole. Before, in a file drawn without line numbers (a `.txt`, or any extension not in `syntax_extensions`), curses moved rows with the terminal's scroll region when a line was inserted or joined, and a terminal that handles that badly showed stale rows until `F4`. The corruption was seen only on Haiku; Linux terminals were not affected. Not yet confirmed on Haiku.
- **First frame, resize, `F4`**: these draw through the same painter as a key press. Before, they showed no status line, dropped word wrap, and cut a line with non-ASCII characters short (sometimes ending in `??`) until the next key.

## What Fails
Priority: P1 function (it does not work), P2 form (it works, but the docs or layout are wrong), P3 tests.

- **P1 Large files**: the whole file is still loaded into memory. There is no lazy load or mmap. led stops when the file exceeds `max_file_size_mb`, or when its lines would take more than `memory_limit_mb`. The buffer stays empty and the status line says `File too large` or `Too many lines`. Each line costs about 90 bytes beyond its text, so with the defaults (10 MB, 50 MB) the size limit is reached first for ordinary source: a 150,000-line, 9.5 MB file opens in about 26 MB. Raise the keys to open larger files.
- **P2 Limits**: `max_file_size_mb` (1–100, default 10), `memory_limit_mb` (1–200, default 50), and `max_line_length` (80–100000, default 10000) are keys in `led.conf`. `max_line_length` cuts nothing: a longer line is loaded whole and shown without syntax colours. A value outside its range keeps the default.
- **P1 Language support**: highlighting and parsing are built for C/C++; other languages may be incomplete. Turn highlighting off in config if a file fights you. Strings, character literals and comments are left plain: brackets and keywords inside them are not coloured and do not nest. A `paired_keywords` pair is used only when both words, or neither, are in `reserved_words`.
- **P2 Error handling**: not fully standardized across modules.
- **P3 No fuzzing harness** yet. Empty-file, size, null-byte, long-line, and memory-ceiling loads are tested.
- **P2 Terminal**: copy and cut reach the system clipboard only where the terminal accepts OSC 52. `xterm` needs `allowWindowOps`, and `tmux` needs `set-clipboard on`; a terminal that does not know the sequence ignores it and the copy stays inside led. led does not read the system clipboard: `Ctrl+V` pastes led's own, and the system's arrives through the terminal's paste key. Selecting with the mouse still copies screen cells (line numbers, border) — use `Ctrl+C` instead. Copies over 100,000 bytes stay inside led.
- **P2 Other languages**: a file such as Python is not highlighted by default. Add its extension to `syntax_extensions` in `colorization.conf` (for example `syntax_extensions=.c,.h,.cpp,.py`) and it is highlighted with the C rules.
- **P1 Garbled display**: seen on Haiku only, in an assembly file (`.s`) with line numbers off, as stale rows after inserting or joining lines. Not reproduced on Linux, and not yet confirmed fixed on Haiku. Press `F4` twice (border on, then off again) to repaint the whole screen; `F2` is not a repaint key. Set `show_line_numbers=1` in `led.conf` to show line numbers in every file.

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
| `Ctrl+A` / `Ctrl+C` / `Ctrl+X` / `Ctrl+V` | Select all / Copy / Cut / Paste (paste is border-filtered). Copy and cut also go to the system clipboard (OSC 52) |
| The terminal's paste key | Paste from the system clipboard: inserted as it is, with no auto-indent, as one undo step. Into an open prompt goes the first line |
| `Ctrl+/` / `Ctrl+R` | Search / Replace (regex, wraps, 100-char limit). `Ctrl+/` is byte 31 or the terminal's modified-key sequence |
| `F2` | Line numbers |
| `F3` | Word wrap |
| `F4` | Border on/off (subwindow inset vs full-width; pasted `│` stripped) |

## Configuration
Two files in `~/.config/led/`, both `key=value` with `#` comments: `led.conf` for general settings and `colorization.conf` for colours and syntax words. led writes each one on first run if it is missing, with every default shown as a comment; remove the `#` from a line to change it. An existing file is never rewritten.

Either file may hold any key, so a `colorization.conf` from before `led.conf` existed keeps working. `led.conf` is read last and wins when both set a key.

`led --prefix DIR file` uses `DIR/led/` in place of `~/.config/led/`, for systems that keep settings somewhere else.

**Colors** (the eight curses colours `BLACK,RED,GREEN,YELLOW,BLUE,MAGENTA,CYAN,WHITE`; the ANSI extended colours as `LIGHT_BLUE` or `BRIGHT_BLUE`, and `GRAY`; `DEFAULT` for the terminal's own colour; or a number `0`–`255`; any letter case): `normal_fg/bg`, `selection_fg/bg`, `semicolon_fg/bg`, `meta_level1..4_fg/bg`, `reserved_words_fg/bg`.

| Number | Curses colour | Number | ANSI extended (led's name) |
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

Curses names eight colours, 0–7, with no light or dark shade. Colours 8–15 are the ANSI extended set, which a terminal with 16 or more colours adds; curses has no names for them, so `LIGHT_` and `BRIGHT_` are led's own. On an 8-colour terminal led shows the matching curses colour in bold instead (see `curs_color(3x)`). A number above what the terminal has falls back to white.

Example, extended colours on the terminal's own background:
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

**Other**: `syntax_extensions=.c,.h,.C,.H,.cpp,.cc,.cxx,.hpp,.hh,.hxx`, `reserved_words=...` (C and C++ keywords), `paired_keywords=` (empty; brackets `()` `[]` `{}` always nest, and word pairs such as FORTH's `if-then,begin-end` can be added), `tab_width=8`, `spaces_for_tab=0`, `show_key_meter=1`, `show_line_numbers=0`, `system_clipboard=1`, `persistent_undo=1`.

**Limits**: `max_file_size_mb=10` (1–100), `memory_limit_mb=50` (1–200), `max_line_length=10000` (80–100000). A file over the size or memory ceiling is not opened. A line longer than `max_line_length` is loaded whole, edited and saved like any other; it is only shown without syntax colours. A null byte is refused with `Binary file`.

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
