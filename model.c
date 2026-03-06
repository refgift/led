#include "model.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <regex.h>

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

void buffer_delete_range(Buffer* buf, size_t start_line, size_t start_col, size_t end_line, size_t end_col) {
    if (start_line > end_line || (start_line == end_line && start_col > end_col)) {
        size_t t = start_line; start_line = end_line; end_line = t;
        t = start_col; start_col = end_col; end_col = t;
    }
    if (start_line == end_line) {
        // delete chars in line
        char* current = buf->lines[start_line];
        size_t len = strlen(current);
        if (start_col >= len) return;
        if (end_col > len) end_col = len;
        char* new_str = malloc(len - (end_col - start_col) + 1);
        memcpy(new_str, current, start_col);
        memcpy(&new_str[start_col], &current[end_col], len - end_col + 1);
        free(buf->lines[start_line]);
        buf->lines[start_line] = new_str;
    } else {
        // delete from start_col to end of start_line
        char* start_line_str = buf->lines[start_line];
        char* new_start = malloc(start_col + 1);
        memcpy(new_start, start_line_str, start_col);
        new_start[start_col] = '\0';
        free(buf->lines[start_line]);
        buf->lines[start_line] = new_start;
        // delete from start of end_line to end_col
        char* end_line_str = buf->lines[end_line];
        size_t end_len = strlen(end_line_str);
        char* new_end = malloc(end_len - end_col + 1);
        memcpy(new_end, &end_line_str[end_col], end_len - end_col + 1);
        free(buf->lines[end_line]);
        buf->lines[end_line] = new_end;
        // delete lines in between
        for (size_t i = 0; i < end_line - start_line - 1; i++) {
            buffer_delete_line(buf, start_line + 1);
        }
        // merge start and end
        char* s = buf->lines[start_line];
        char* e = buf->lines[start_line + 1];
        size_t sl = strlen(s);
        size_t el = strlen(e);
        char* merged = malloc(sl + el + 1);
        memcpy(merged, s, sl);
        memcpy(&merged[sl], e, el + 1);
        free(buf->lines[start_line]);
        free(buf->lines[start_line + 1]);
        buf->lines[start_line] = merged;
        for (size_t i = start_line + 1; i < buf->num_lines - 1; i++) {
            buf->lines[i] = buf->lines[i + 1];
        }
        buf->num_lines--;
    }
}

size_t buffer_num_lines(const Buffer* buf) {
    return buf->num_lines;
}

const char* buffer_get_line(const Buffer* buf, size_t line) {
    if (line >= buf->num_lines) return NULL;
    return buf->lines[line];
}

size_t buffer_get_line_length(const Buffer* buf, size_t line) {
    if (line >= buf->num_lines) return 0;
    return strlen(buf->lines[line]);
}

char buffer_get_char(const Buffer* buf, size_t line, size_t col) {
    if (line >= buf->num_lines) return '\0';
    const char* ln = buf->lines[line];
    size_t len = strlen(ln);
    if (col >= len) return '\0';
    return ln[col];
}

void buffer_insert_line(Buffer* buf, size_t line, const char* content) {
    if (line > buf->num_lines) return;
    if (buf->num_lines >= buf->capacity) {
        buf->capacity = buf->capacity == 0 ? INITIAL_LINES_CAPACITY : buf->capacity * 2;
        buf->lines = realloc(buf->lines, buf->capacity * sizeof(char*));
        if (!buf->lines) return;
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
    char* ln = buf->lines[line];
    size_t len = strlen(ln);
    if (col > len) col = len;
    char* new_ln = malloc(len + 2);
    if (!new_ln) return;
    memcpy(new_ln, ln, col);
    new_ln[col] = c;
    memcpy(&new_ln[col + 1], &ln[col], len - col + 1);
    free(buf->lines[line]);
    buf->lines[line] = new_ln;
}

void buffer_insert_text(Buffer* buf, size_t line, size_t col, const char* text) {
    if (line >= buf->num_lines) return;
    char* ln = buf->lines[line];
    size_t len = strlen(ln);
    size_t text_len = strlen(text);
    if (col > len) col = len;
    char* new_ln = malloc(len + text_len + 1);
    if (!new_ln) return;
    memcpy(new_ln, ln, col);
    memcpy(&new_ln[col], text, text_len);
    memcpy(&new_ln[col + text_len], &ln[col], len - col + 1);
    free(buf->lines[line]);
    buf->lines[line] = new_ln;
}

int buffer_save_to_file(const Buffer* buf, const char* filename) {
    FILE* fp = fopen(filename, "wb");
    if (!fp) return -1;
    for (size_t i = 0; i < buf->num_lines; i++) {
        fputs(buf->lines[i], fp);
        if (i < buf->num_lines - 1) fputc('\n', fp);
    }
    fclose(fp);
    return 0;
}

void buffer_replace_all(Buffer* buf, const char* search_regex, const char* replace_str) {
    regex_t reg;
    if (regcomp(&reg, search_regex, REG_EXTENDED) != 0) {
        return;
    }
    for (size_t i = 0; i < buf->num_lines; i++) {
        char* line = buf->lines[i];
        size_t len = strlen(line);
        char* new_line = malloc(len * 2 + strlen(replace_str) * 10 + 1); // rough estimate
        if (!new_line) continue;
        new_line[0] = 0;
        size_t pos = 0;
        regmatch_t match;
        while (regexec(&reg, line + pos, 1, &match, 0) == 0) {
            // append before match
            strncat(new_line, line + pos, match.rm_so);
            // append replace
            strcat(new_line, replace_str);
            pos += match.rm_eo;
        }
        // append rest
        strcat(new_line, line + pos);
        // replace
        free(buf->lines[i]);
        buf->lines[i] = new_line;
    }
    regfree(&reg);
}