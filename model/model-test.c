#include "model.h"
#include <assert.h>
#include <string.h>
#include <stdlib.h>

int main(void) {
    Buffer buf; buffer_init(&buf);
    assert(buffer_insert_line(&buf, 0, "hello") == 0);
    assert(buffer_get_line_length(&buf, 0) == 5);
    buffer_insert_char(&buf, 0, 5, '\n');   // Split line
    assert(buffer_num_lines(&buf) == 2);
    assert(buffer_insert_text(&buf, 1, 0, "world") == 0);
    char * line = buffer_get_line(&buf, 1);
    assert(strcmp(line, "world") == 0);
    free(line);
    buffer_save_to_file(&buf, "tmp.txt");
    buffer_free(&buf);
    return 0;
}