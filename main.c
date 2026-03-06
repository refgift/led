#include <ncurses.h>
#include "editor.h"
#include "view.h"
#include "config.h"

int main(int argc, char *argv[]) {
    // Initialize curses
    initscr();
    raw();
    noecho();
    keypad(stdscr, TRUE);
    start_color();

    Editor ed;
    editor_init(&ed, argc, argv);

    // Initialize color pairs
    init_pair(1, ed.config.normal_fg, ed.config.normal_bg);
    init_pair(2, ed.config.selection_fg, ed.config.selection_bg);
    init_pair(3, ed.config.semicolon_fg, ed.config.semicolon_bg);
    init_pair(4, ed.config.meta_level1_fg, ed.config.meta_level1_bg);
    init_pair(5, ed.config.meta_level2_fg, ed.config.meta_level2_bg);
    init_pair(6, ed.config.meta_level3_fg, ed.config.meta_level3_bg);
    init_pair(7, ed.config.meta_level4_fg, ed.config.meta_level4_bg);
    init_pair(8, ed.config.reserved_words_fg, ed.config.reserved_words_bg);

    draw_initial(stdscr, &ed.model, ed.scroll_row, ed.scroll_col, ed.cursor_line, ed.cursor_col, ed.show_line_numbers, ed.syntax_highlight, &ed.config);

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