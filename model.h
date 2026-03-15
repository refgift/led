#ifndef BUFFER_H
#define BUFFER_H

#include <stddef.h>

#define INITIAL_LINES_CAPACITY 10

typedef struct {
    char** lines;
    int num_lines;
    int capacity;
} Buffer;

void buffer_init(Buffer* buf);
void buffer_free(Buffer* buf);
int buffer_load_from_file(Buffer* buf, const char* filename);
int buffer_save_to_file(const Buffer* buf, const char* filename);
const char* buffer_get_line(const Buffer* buf, int line);
int buffer_get_line_length(const Buffer* buf, int line);
int buffer_num_lines(const Buffer* buf);
char buffer_get_char(const Buffer* buf, int line, int col);
int buffer_insert_line(Buffer* buf, int line, const char* content);
int buffer_delete_line(Buffer* buf, int line);
int buffer_insert_char(Buffer* buf, int line, int col, char c);
int buffer_delete_char(Buffer* buf, int line, int col);
int buffer_delete_range(Buffer* buf, int start_line, int start_col, int end_line, int end_col);
int buffer_insert_text(Buffer* buf, int line, int col, const char* text);
void buffer_replace_all(Buffer* buf, const char* search_regex, const char* replace_str);

#endif