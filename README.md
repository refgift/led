# led - Larry's Editor for Linux/Unix Terminal (Version 1.0.0)
> See [WARNINGS.md](WARNINGS.md) for known limitations and safety notes. 

## Dedication
This editor is dedicated to the memory of Neal Stephenson, whose vision of a 
word processor that truly serves authors inspired its development. Stephenson 
lamented that computer editors delete as easily as they create, unlike paper 
which preserves every word. This editor strives to provide the safety and 
reliability of paper in the digital realm, ensuring no work is lost to 
accidents or crashes.

## Philosophy
In the philosophy of software design, code exists as both map and territory. 
The map is the source code we write; the territory is the executing program 
that shapes our digital reality. Led embodies this duality by prioritizing 
constraints that govern the territory of editing: safety, reliability, and user 
sovereignty.

Led invites developers who see software as a craft of reliable tools, not 
disposable artifacts. Contributors are welcome to extend this vision, ensuring 
that digital editing respects the user's work as paper once did: permanent, 
safe, and under their control.

## Recent Code Review and Fixes

Following a thorough code review of led version 1.0.0, several critical security vulnerabilities, memory management issues, and reliability problems have been addressed. The review prioritized fixes that prevent data loss, crashes, and security exploits, aligning with the editor's philosophy of data safety.

### Completed Fixes

#### Priority 1 (Critical - Addressed Immediately)
These fixes prevent data loss, crashes, and security vulnerabilities:
- **Buffer Overflow in File Loading**: Added pre-loading file size validation using `stat()` and line length limits (10,000 characters) to prevent excessive memory allocation from malformed files.
- **Memory Exhaustion on Allocation Failure**: Replaced abrupt `exit()` calls with graceful error handling, allowing the editor to continue or prompt for saves instead of losing unsaved work.
- **Logic Error in Range Deletion**: Added strict range validation to prevent corrupted buffer states during text deletion operations.
- **Unsafe Regex Usage**: Limited search/replace patterns to 100 characters and added safe compilation flags to prevent DoS attacks from malicious regex patterns.

#### Priority 2 (High/Medium - Addressed for Stability)
These improvements enhance reliability and performance:
- **Memory Leak in Undo/Redo Stack**: Capped undo/redo operations at 10,000 entries and fixed memory cleanup to prevent resource exhaustion.
- **Double-Free Risks**: Confirmed and maintained atomic buffer operations to prevent crashes during complex edits.
- **Cursor and editing stability**: Improved tab expansion and cursor positioning.
- **Unsafe String Operations**: Replaced fixed-size buffers with dynamic allocation in syntax highlighting to handle long identifiers correctly.
- **Config Parsing Vulnerability**: Switched from unsafe `strncpy` to `snprintf` for configuration value copying to prevent buffer overflows.

### Remaining Work

While significant progress has been made, the following items remain pending and are scheduled for future updates:

#### Priority 2 (Medium - Stability Enhancements)
- **Efficient File Loading**: Implement lazy line loading or memory mapping for better performance with very large files (currently loads entire files into memory).
- **Configurable Limits**: Move hardcoded limits (file size, line length) to user configuration for flexibility.
- **Error Handling Standardization**: Improve consistency of error codes and user feedback across all modules.
- **Global State Removal**: Move undo/redo stacks from global to per-editor instances for better multi-instance support.

#### Priority 3 (Low - Quality Improvements)
- **Code Duplication Reduction**: Extract common cursor movement logic into utility functions.
- **Comprehensive Testing**: Add fuzzing tests and coverage for extreme edge cases (empty files, very long lines).
- **Input Sanitization**: Validate filename arguments to prevent directory traversal attacks.
- **Performance Optimizations**: Implement color caching for syntax highlighting to reduce recalculations.

### Impact
These fixes significantly improve led's reliability as a data-safe editor, addressing the core concerns raised in the review while maintaining the editor's performance and feature set. The remaining work focuses on scalability and polish, ensuring led continues to evolve as a robust terminal editor.

This is led version 1.0.0, a powerful and reliable terminal-based text 
editor for Linux/Unix, developed with the assistance of AI. 
It features a robust model-view-controller architecture with advanced 
capabilities including unlimited undo/redo, configurable auto-save with 
versioned backups, syntax highlighting for C/C++ code with multi-line nesting support, comprehensive error 
handling for data safety, and a configurable user interface. This release 
represents a significant advancement in terminal editor technology, balancing 
performance with user-friendly features inspired by the need for paper-like 
data preservation in digital editing.

## Key Features
- **Data Safety First**: Unlimited undo/redo, auto-save with configurable 
triggers and versioned backups, crash recovery, file validation to prevent data 
loss.
- **Advanced Editing**: Syntax highlighting with nesting-based color intensity 
for C/C++, search and replace with regex support, selection and clipboard 
operations.
- **User Experience**: Configurable status bar with version/time/key-response-meter/filename display, extended ASCII support, intuitive keyboard shortcuts, clear error feedback.
- **Reliability**: Memory error handling with user notifications, graceful 
degradation under low-memory conditions, robust buffer operations.
- **Performance**: Efficient rendering for large files (up to 10MB), modular 
architecture for maintainability.
- **Test Suite**: Expanded to 95 tests covering edge cases and large files.

## Dependencies
The code is in C language and needs a C compiler and build chain.
It comes with a Makefile which needs the make utility to use.
It uses the libncurses development files to perform TUI work.
Uses glibc regex for search and replace operations.

## Build
make

## Run
./led filename.c

## Test Mode
./led -t filename.c  # Loads file, runs automated tests on 
model/view/controller, outputs to stderr, exits

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
- Ctrl+R: Enter replace mode (type search regex, Enter, type replace string, 
Enter to replace all matches, Esc to cancel)

### Display
- F2: Toggle line number display
- Key response meter (μs) in status bar (configurable via show_key_meter)
- Syntax highlighting for meta symbols (;, braces, etc.) with nesting-based 
color intensity

## Configuration
The editor supports customizable colors and syntax highlighting via a 
configuration file located at `~/.config/led/colorization.conf`. This file uses 
a simple key-value format with one setting per line. Lines starting with `#` 
are comments. If the file doesn't exist, default settings are used.

### Color Options
Colors are specified using standard ncurses color names: BLACK, RED, GREEN, 
YELLOW, BLUE, MAGENTA, CYAN, WHITE.
- `normal_fg` / `normal_bg`: Foreground and background colors for normal text 
(default: WHITE/BLACK)
- `selection_fg` / `selection_bg`: Colors for selected text (default: 
CYAN/BLACK)
- `semicolon_fg` / `semicolon_bg`: Colors for semicolons (default: RED/BLACK)
- `meta_level1_fg` / `meta_level1_bg` to `meta_level4_fg` / `meta_level4_bg`: 
Colors for nested meta-symbols (braces, brackets, parentheses, commas). Level 1 
is outermost, increasing for deeper nesting (defaults: BLUE/BLACK for level 1, 
CYAN/BLACK for level 2, GREEN/BLACK for level 3, YELLOW/BLACK for level 4+)

### Syntax Highlighting
- `reserved_words_fg` / `reserved_words_bg`: Colors for reserved words/keywords 
(default: RED/BLACK)
- `reserved_words`: Comma-separated list of reserved words to highlight 
(default: common C keywords)
- `paired_keywords`: Comma-separated list of keyword/symbol pairs in open-close 
format (e.g., if-then,begin-end,(,)) to highlight with nesting colors (default: 
if-then,begin-end,(,))
- `syntax_extensions`: Comma-separated list of file extensions that enable 
syntax highlighting (default: .c,.h,.cpp)

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
reserved_words=int,char,return,if,else,for,while,do,switch,case,default,break,co
ntinue,goto,sizeof,typedef,struct,union,enum,static,extern,auto,register,volatil
e,const,signed,unsigned,short,long,double,float,void
paired_keywords=if-then,begin-end,(,)
syntax_extensions=.c,.h,.cpp
show_key_meter=1
```
(Note: show_key_meter controls the | Xus response time meter in status bar.)
Changes take effect the next time you start the editor. The configuration 
directory `~/.config/led/` is created automatically if it doesn't exist.