#ifndef BUFFER_H
#define BUFFER_H

#include <stddef.h>

#define INITIAL_LINES_CAPACITY 10UL

typedef struct {
    char** lines;
    size_t num_lines;
    size_t capacity;
} Buffer;

void buffer_init(Buffer* buf);
void buffer_free(Buffer* buf);
int buffer_load_from_file(Buffer* buf, const char* filename);
int buffer_save_to_file(const Buffer* buf, const char* filename);
const char* buffer_get_line(const Buffer* buf, size_t line);
size_t buffer_get_line_length(const Buffer* buf, size_t line);
size_t buffer_num_lines(const Buffer* buf);
char buffer_get_char(const Buffer* buf, size_t line, size_t col);
int buffer_insert_line(Buffer* buf, size_t line, const char* content);
int buffer_delete_line(Buffer* buf, size_t line);
int buffer_insert_char(Buffer* buf, size_t line, size_t col, char c);
int buffer_delete_char(Buffer* buf, size_t line, size_t col);
int buffer_delete_range(Buffer* buf, size_t start_line, size_t start_col, size_t end_line, size_t end_col);
int buffer_insert_text(Buffer* buf, size_t line, size_t col, const char* text);
void buffer_replace_all(Buffer* buf, const char* search_regex, const char* replace_str);

#endif