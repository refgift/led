#pragma once

void run_comprehensive_tests(void);
extern int tests_passed;
extern int tests_failed;
void run_all_tests(void); // legacy alias if needed
void test_buffer_init_free(void);
void test_buffer_load_save(void);
void test_buffer_manipulation(void);
void test_search_functionality(void);
void test_buffer_replace_all(void);
void test_undo_redo_comprehensive(void);
void test_clipboard_comprehensive(void);
void test_ctrl_x_last_line(void);
void test_enter_key_newline_insertion(void);
void test_right_arrow_repeat_navigation(void);
void test_cursor_newline_undo_redo_bug(void);
void run_view_tests(void);
void test_autosave_comprehensive(void);