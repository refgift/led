# led - Larry's Editor for Linux/Unix Terminal
I have been wanting my own editor for a long time and
with grok-code-fast-1 helping I got though the gate
and made a model-view-controller architecture
designed text editor with just basic functionality
with this first checkin.

## Dependencies
The code is in C language and needs a C compiler and build chain.
It comes with a Makefile which needs the make utility to use.
It uses the libncurses development files to perform TUI work.

## Build
make

## Run
./led filename.c

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
- Backspace/Delete: Delete character
- Ctrl+Z: Undo last operation

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
- Syntax highlighting for meta symbols (;, braces, etc.) with nesting-based color intensity

