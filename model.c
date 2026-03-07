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

    // Safety checks
    size_t max_size = 10 * 1024 * 1024; // 10MB default, could be configurable
    if (pos > max_size) {
        free(temp);
        return -1; // File too large
    }

    // Check for null bytes (indicates binary file)
    for (size_t i = 0; i < pos; i++) {
        if (temp[i] == '\0') {
            free(temp);
            return -1; // Binary file or null bytes
        }
    }

    // Split into lines
    size_t start = 0;
    for (size_t i = 0; i <= pos; i++) {
        if (temp[i] == '\n' || temp[i] == '\0') {
            size_t len = i - start;
            if (len > 0) {
                char* line = malloc(len + 1);
                if (!line) { free(temp); return -1; }
                memcpy(line, &temp[start], len);
                line[len] = '\0';
                buffer_insert_line(buf, buf->num_lines, line);
                free(line);
            }
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
        if (buf->capacity == 0) buf->capacity = INITIAL_LINES_CAPACITY;
        else buf->capacity *= 2;
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
    if (c == '\n') {
        // split line at col
        char* ln = buf->lines[line];
        // part before col
        char* before = malloc(col + 1);
        if (!before) return;
        memcpy(before, ln, col);
        before[col] = '\0';
        // part after col
        char* after = my_strdup(ln + col);
        // replace current line
        free(buf->lines[line]);
        buf->lines[line] = before;
        // insert new line after
        buffer_insert_line(buf, line + 1, after);
        free(after);
    } else {
        // insert char
        char* ln = buf->lines[line];
        size_t len = strlen(ln);
        if (col > len) col = len;
        char* new_ln = malloc(len + 2);
        if (!new_ln) return;
        memcpy(new_ln, ln, col);
        new_ln[col] = c;
        memcpy(&new_ln[col + 1], ln + col, len - col + 1);
        free(buf->lines[line]);
        buf->lines[line] = new_ln;
    }
}

void buffer_insert_text(Buffer* buf, size_t line, size_t col, const char* text) {
    if (line >= buf->num_lines) return;
    // Insert text, handling \n by splitting lines
    const char* p = text;
    size_t current_line = line;
    size_t current_col = col;
    while (*p) {
        if (*p == '\n') {
            // insert newline
            buffer_insert_char(buf, current_line, current_col, '\n');
            current_line++;
            current_col = 0;
            p++;
        } else {
            // find next \n or end
            const char* end = p;
            while (*end && *end != '\n') end++;
            size_t len = end - p;
            // insert substring
            char* ln = buf->lines[current_line];
            size_t ln_len = strlen(ln);
            if (current_col > ln_len) current_col = ln_len;
            char* new_ln = malloc(ln_len + len + 1);
            if (!new_ln) return;
            memcpy(new_ln, ln, current_col);
            memcpy(new_ln + current_col, p, len);
            memcpy(new_ln + current_col + len, ln + current_col, ln_len - current_col + 1);
            free(buf->lines[current_line]);
            buf->lines[current_line] = new_ln;
            current_col += len;
            p = end;
        }
    }
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
    size_t replace_len = strlen(replace_str);
    for (size_t i = 0; i < buf->num_lines; i++) {
        char* line = buf->lines[i];
        size_t len = strlen(line);
        // Count matches to estimate size
        size_t match_count = 0;
        size_t pos = 0;
        regmatch_t match;
        while (regexec(&reg, line + pos, 1, &match, 0) == 0) {
            match_count++;
            pos += match.rm_eo;
        }
        size_t new_len = len + match_count * (replace_len - 1) + 1; // rough
        char* new_line = malloc(new_len);
        if (!new_line) continue;
        new_line[0] = 0;
        pos = 0;
        size_t used = 0;
        while (regexec(&reg, line + pos, 1, &match, 0) == 0) {
            // append before
            size_t before_len = (size_t)match.rm_so;
            if (used + before_len >= new_len) {
                new_len *= 2;
                new_line = realloc(new_line, new_len);
                if (!new_line) break;
            }
            memcpy(new_line + used, line + pos, before_len);
            used += before_len;
            // append replace
            if (used + replace_len >= new_len) {
                new_len *= 2;
                new_line = realloc(new_line, new_len);
                if (!new_line) break;
            }
            memcpy(new_line + used, replace_str, replace_len);
            used += replace_len;
            pos += match.rm_eo;
        }
        // append rest
        size_t rest_len = len - pos;
        if (used + rest_len >= new_len) {
            new_len = used + rest_len + 1;
            new_line = realloc(new_line, new_len);
            if (!new_line) continue;
        }
        memcpy(new_line + used, line + pos, rest_len);
        used += rest_len;
        new_line[used] = '\0';
        // replace
        free(buf->lines[i]);
        buf->lines[i] = new_line;
    }
    regfree(&reg);
}