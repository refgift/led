#pragma once

#include <ncurses.h>
#include <time.h>
#include "model.h"
#include "config.h"

typedef struct {
    Buffer model;
    int scroll_row;
    int scroll_col;
    int show_line_numbers;
    int syntax_highlight;
    int cursor_line;
    int cursor_col;
    int selection_start_line;
    int selection_start_col;
    int selection_end_line;
    int selection_end_col;
    int selection_active;
    char search_buffer[256];
    int search_mode;
    char replace_buffer[256];
    int replace_step;
    char* clipboard;
    const char* filename;
    EditorConfig config;
    // Auto-save fields
    int unsaved_keystrokes;
    int auto_save_threshold;  // keystrokes before auto-save
    int auto_save_timeout;    // seconds before time-based auto-save
    time_t last_save_time;
    int backup_count;

    // Status bar
    char status_message[256];
    time_t status_message_time;

    // File status
    int file_modified;
    
    // Visual rendering state (for word wrap support)
    int total_visual_lines;  // Total visual lines when word_wrap ON
} Editor;

void editor_init(Editor* ed, int argc, char* argv[]);
void editor_draw(WINDOW* win, Editor* ed);
void editor_handle_input(Editor* ed, int ch);
void editor_cleanup(Editor* ed);
void auto_save(Editor* ed);
void set_status_message(Editor* ed, const char* message);