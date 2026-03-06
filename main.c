#include <ncurses.h>
#include "editor.h"
#include "view.h"

int main(int argc, char *argv[]) {
    // Initialize curses
    initscr();
    raw();
    noecho();
    keypad(stdscr, TRUE);
    start_color();

    // Initialize color pairs
    init_pair(1, COLOR_WHITE, COLOR_BLACK);
    init_pair(2, COLOR_CYAN, COLOR_BLACK);
    init_pair(3, COLOR_RED, COLOR_BLACK);
    init_pair(4, COLOR_BLUE, COLOR_BLACK);
    init_pair(5, COLOR_GREEN, COLOR_BLACK);
    init_pair(6, COLOR_YELLOW, COLOR_BLACK);

    Editor ed;
    editor_init(&ed, argc, argv);

    draw_initial(stdscr, &ed.model, ed.scroll_row, ed.scroll_col, ed.cursor_line, ed.cursor_col, ed.show_line_numbers);

    while (1) {
        int ch = getch();
        if (ch == 17) break; // Ctrl+Q
        editor_handle_input(&ed, ch);
        editor_draw(stdscr, &ed);
    }

    editor_cleanup(&ed);
    endwin();
    return 0;
}