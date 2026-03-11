# led - Larry's Editor for Linux/Unix Terminal
> **Warning**: See [WARNINGS.md](WARNINGS.md) for known issues, limitations, and safety notes.
## Dedication
This editor is dedicated to the memory of Neal Stephenson, whose vision of a word processor that truly serves authors inspired its development. Stephenson lamented that computer editors delete as easily as they create, unlike paper which preserves every word. This editor strives to provide the safety and reliability of paper in the digital realm, ensuring no work is lost to accidents or crashes.
## Philosophy
In the philosophy of software design, code exists as both map and territory. The map is the source code we write; the territory is the executing program that shapes our digital reality. Led embodies this duality by prioritizing constraints that govern the territory of editing: safety, reliability, and user sovereignty.
Led invites developers who see software as a craft of reliable tools, not disposable artifacts. Contributors are welcome to extend this vision, ensuring that digital editing respects the user's work as paper once did—permanent, safe, and under their control.
This is led, a powerful and reliable terminal-based text editor for Linux/Unix, developed with the assistance of grok-code-fast-1. It features a robust model-view-controller architecture with advanced capabilities including unlimited undo/redo, configurable auto-save with versioned backups, syntax highlighting for C/C++ code, comprehensive error handling for data safety, and a configurable user interface. This release represents a significant advancement in terminal editor technology, balancing performance with user-friendly features inspired by the need for paper-like data preservation in digital editing.
## Key Features
- **Data Safety First**: Unlimited undo/redo, auto-save with configurable triggers and versioned backups, crash recovery, file validation to prevent data loss.
- **Advanced Editing**: Syntax highlighting with nesting-based color intensity for C/C++, search and replace with regex support, selection and clipboard operations, word wrap toggle.
- **User Experience**: Configurable status bar with version/time/filename display, extended ASCII support, intuitive keyboard shortcuts, clear error feedback.
- **Reliability**: Memory error handling with user notifications, graceful degradation under low-memory conditions, robust buffer operations.
- **Performance**: Efficient rendering for large files (up to 10MB), modular architecture for maintainability.
## Dependencies
The code is in C language and needs a C compiler and build chain.
It comes with a Makefile which needs the make utility to use.
It uses the libncurses development files to perform TUI work.
It also requires the rx library for regex functionality in search and replace operations.
## Build
make
## Run
./led filename.c
## Test Mode
./led -t filename.c  # Loads file, runs automated tests on model/view/controller, outputs to stderr, exits
## Usage
The editor is controlled via keyboard shortcuts and arrow keys.
### Navigation
- Arrow keys: Move cursor up, down, left, right
- Home: Move to start of line (press twice for top of file)
- End: Move to end of line (press twice for bottom of file)
- Page Up: Scroll up one page
- Page Down: Scroll down one page
### Editing
- Type printable characters to insert text
- Enter: Insert new line
- Backspace/Delete: Delete character (forward/backward)
- Ctrl+Z: Undo last operation
- Ctrl+Y: Redo last undone operation
### File Operations
- Ctrl+S: Save file
- Ctrl+Q: Quit editor
### Selection and Clipboard
- Ctrl+A: Toggle selection mode (drag to select text)
- Ctrl+C: Copy selected text (or current line if no selection)
- Ctrl+X: Cut selected text (or current line if no selection)
- Ctrl+V: Paste clipboard content
### Search
- Ctrl+/: Enter search mode (type regex pattern, Enter to search, Esc to cancel)
### Replace
- Ctrl+R: Enter replace mode (type search regex, Enter, type replace string, Enter to replace all matches, Esc to cancel)
### Display
- F2: Toggle line number display
- F3: Toggle word wrap (visual soft wrap of long lines)
- Syntax highlighting for meta symbols (;, braces, etc.) with nesting-based color intensity
## Configuration
The editor supports customizable colors and syntax highlighting via a configuration file located at `~/.config/led/colorization.conf`. This file uses a simple key-value format with one setting per line. Lines starting with `#` are comments. If the file doesn't exist, default settings are used.
### Color Options
Colors are specified using standard ncurses color names: BLACK, RED, GREEN, YELLOW, BLUE, MAGENTA, CYAN, WHITE.
- `normal_fg` / `normal_bg`: Foreground and background colors for normal text (default: WHITE/BLACK)
- `selection_fg` / `selection_bg`: Colors for selected text (default: CYAN/BLACK)
- `semicolon_fg` / `semicolon_bg`: Colors for semicolons (default: RED/BLACK)
- `meta_level1_fg` / `meta_level1_bg` to `meta_level4_fg` / `meta_level4_bg`: Colors for nested meta-symbols (braces, brackets, parentheses, commas). Level 1 is outermost, increasing for deeper nesting (defaults: BLUE/BLACK for level 1, CYAN/BLACK for level 2, GREEN/BLACK for level 3, YELLOW/BLACK for level 4+)
### Syntax Highlighting
- `reserved_words_fg` / `reserved_words_bg`: Colors for reserved words/keywords (default: RED/BLACK)
- `reserved_words`: Comma-separated list of reserved words to highlight (default: common C keywords)
- `paired_keywords`: Comma-separated list of keyword/symbol pairs in open-close format (e.g., if-then,begin-end,(,)) to highlight with nesting colors (default: if-then,begin-end,(,))
- `syntax_extensions`: Comma-separated list of file extensions that enable syntax highlighting (default: .c,.h,.cpp)
### Example Configuration
```
# Color configuration for led editor
# Colors: BLACK, RED, GREEN, YELLOW, BLUE, MAGENTA, CYAN, WHITE
normal_fg=WHITE
normal_bg=BLACK
selection_fg=CYAN
selection_bg=BLACK
semicolon_fg=RED
semicolon_bg=BLACK
meta_level1_fg=BLUE
meta_level1_bg=BLACK
meta_level2_fg=CYAN
meta_level2_bg=BLACK
meta_level3_fg=GREEN
meta_level3_bg=BLACK
meta_level4_fg=YELLOW
meta_level4_bg=BLACK
reserved_words_fg=RED
reserved_words_bg=BLACK
reserved_words=int,char,return,if,else,for,while,do,switch,case,default,break,continue,goto,sizeof,typedef,struct,union,enum,static,extern,auto,register,volatile,const,signed,unsigned,short,long,double,float,void
paired_keywords=if-then,begin-end,(,)
syntax_extensions=.c,.h,.cpp
word_wrap=false
```
Changes take effect the next time you start the editor. The configuration directory `~/.config/led/` is created automatically if it doesn't exist.