#include "test_helpers.h"
#include "controller.h"

#include <string.h>

void test_init(TestContext *ctx)
{
    if (!ctx) return;

    memset(ctx, 0, sizeof(*ctx));

    test_reset_undo(&ctx->ed);

    buffer_init(&ctx->buf);
    ctx->filename = NULL;
    ctx->ed.config.display.tab_width = 8;
}

void test_cleanup(TestContext *ctx)
{
    if (!ctx) return;

    buffer_free(&ctx->buf);

    if (ctx->clipboard) {
        free(ctx->clipboard);
        ctx->clipboard = NULL;
    }

    test_reset_undo(&ctx->ed);
}

void test_reset_undo(Editor *ed)
{
    if (ed)
        free_undo_stacks(&ed->undo_stack, &ed->redo_stack);
}

int test_handle_input(TestContext *ctx, int ch)
{
    if (!ctx) return -1;
    return handle_input(ch, &ctx->buf, &ctx->scroll_row, &ctx->scroll_col,
                        &ctx->cursor_line, &ctx->cursor_col,
                        &ctx->show_line_numbers, ctx->search_buffer,
                        &ctx->search_mode, &ctx->clipboard,
                        ctx->filename, &ctx->ed);
}
