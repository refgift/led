#include "model.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static char* my_strdup(const char* s) {
    size_t len = strlen(s);
    char* d = malloc(len + 1);
    if (d) memcpy(d, s, len + 1);
    return d;
}

void buffer_init(Buffer* buf) {
    buf->lines = NULL;
    buf->num_lines = 0;
    buf->capacity = 0;
}

void buffer_free(Buffer* buf) {
    for (size_t i = 0; i < buf->num_lines; i++) {
        free(buf->lines[i]);
    }
    free(buf->lines);
    buf->lines = NULL;
    buf->num_lines = 0;
    buf->capacity = 0;
}

int buffer_load_from_file(Buffer* buf, const char* filename) {
    FILE* fp = fopen(filename, "rb");
    if (!fp) return -1;

    // Read entire file into temp buffer
    size_t temp_cap = 1024;
    char* temp = malloc(temp_cap);
    if (!temp) return -1;
    size_t pos = 0;
    int c;
    while ((c = fgetc(fp)) != EOF) {
        if (pos >= temp_cap) {
            temp_cap *= 2;
            temp = realloc(temp, temp_cap);
            if (!temp) return -1;
        }
        temp[pos++] = c;
    }
    fclose(fp);
    temp[pos] = '\0';

    // Split into lines
    size_t start = 0;
    for (size_t i = 0; i <= pos; i++) {
        if (temp[i] == '\n' || temp[i] == '\0') {
            size_t len = i - start;
            char* line = malloc(len + 1);
            if (!line) { free(temp); return -1; }
            memcpy(line, &temp[start], len);
            line[len] = '\0';
            buffer_insert_line(buf, buf->num_lines, line);
            free(line);
            start = i + 1;
        }
    }
    free(temp);
    return 0;
}

int buffer_save_to_file(const Buffer* buf, const char* filename) {
    FILE* fp = fopen(filename, "w");
    if (!fp) return -1;
    for (size_t i = 0; i < buf->num_lines; i++) {
        fputs(buf->lines[i], fp);
        if (i < buf->num_lines - 1) fputc('\n', fp);
    }
    fclose(fp);
    return 0;
}

const char* buffer_get_line(const Buffer* buf, size_t line) {
    if (line >= buf->num_lines) return "";
    return buf->lines[line];
}

size_t buffer_get_line_length(const Buffer* buf, size_t line) {
    if (line >= buf->num_lines) return 0;
    return strlen(buf->lines[line]);
}

size_t buffer_num_lines(const Buffer* buf) {
    return buf->num_lines;
}

char buffer_get_char(const Buffer* buf, size_t line, size_t col) {
    if (line >= buf->num_lines) return '\0';
    const char* l = buf->lines[line];
    size_t len = strlen(l);
    if (col < len) return l[col];
    return '\0';
}

void buffer_insert_line(Buffer* buf, size_t line, const char* content) {
    if (line > buf->num_lines) line = buf->num_lines;
    if (buf->num_lines >= buf->capacity) {
        buf->capacity = buf->capacity ? buf->capacity * 2 : INITIAL_LINES_CAPACITY;
        buf->lines = realloc(buf->lines, buf->capacity * sizeof(char*));
        if (!buf->lines) exit(1);
    }
    for (size_t i = buf->num_lines; i > line; i--) {
        buf->lines[i] = buf->lines[i - 1];
    }
    buf->lines[line] = my_strdup(content);
    buf->num_lines++;
}

void buffer_delete_line(Buffer* buf, size_t line) {
    if (line >= buf->num_lines) return;
    free(buf->lines[line]);
    for (size_t i = line; i < buf->num_lines - 1; i++) {
        buf->lines[i] = buf->lines[i + 1];
    }
    buf->num_lines--;
}

void buffer_insert_char(Buffer* buf, size_t line, size_t col, char c) {
    if (line >= buf->num_lines) return;
    if (c == '\n') {
        // Split line
        const char* current = buf->lines[line];
        size_t len = strlen(current);
        char* new_line = malloc(col + 1);
        if (!new_line) return;
        memcpy(new_line, current, col);
        new_line[col] = '\0';
        char* rest = malloc(len - col + 1);
        if (!rest) { free(new_line); return; }
        memcpy(rest, &current[col], len - col + 1);
        free(buf->lines[line]);
        buf->lines[line] = new_line;
        buffer_insert_line(buf, line + 1, rest);
        free(rest);
    } else {
        // Insert char
        const char* current = buf->lines[line];
        size_t len = strlen(current);
        char* new_str = malloc(len + 2);
        if (!new_str) return;
        memcpy(new_str, current, col);
        new_str[col] = c;
        memcpy(&new_str[col + 1], &current[col], len - col + 1);
        free(buf->lines[line]);
        buf->lines[line] = new_str;
    }
}

void buffer_delete_char(Buffer* buf, size_t line, size_t col) {
    if (line >= buf->num_lines) return;
    size_t len = strlen(buf->lines[line]);
    if (col < len) {
        // Delete char in line
        char* current = buf->lines[line];
        char* new_str = malloc(len);
        if (!new_str) return;
        memcpy(new_str, current, col);
        memcpy(&new_str[col], &current[col + 1], len - col);
        free(buf->lines[line]);
        buf->lines[line] = new_str;
    } else if (line < buf->num_lines - 1) {
        // Merge with next line
        char* current = buf->lines[line];
        char* next = buf->lines[line + 1];
        size_t len1 = strlen(current);
        size_t len2 = strlen(next);
        char* merged = malloc(len1 + len2 + 1);
        if (!merged) return;
        memcpy(merged, current, len1);
        memcpy(&merged[len1], next, len2 + 1);
        free(buf->lines[line]);
        free(buf->lines[line + 1]);
        buf->lines[line] = merged;
        for (size_t i = line + 1; i < buf->num_lines - 1; i++) {
            buf->lines[i] = buf->lines[i + 1];
        }
        buf->num_lines--;
    }
}