# Dependencies: ncursesw (UTF-8 / wide-character terminal UI)
CC = cc
CFLAGS = -I. -Iview -Ifrontend -Itest -Imodel -Icontroller -Iconfig -Iutils -D_DEFAULT_SOURCE -D_XOPEN_SOURCE=700 -Wall -Wextra -Wno-unused-parameter -std=c11 -O3
LDFLAGS =
# Front end: curses (default) or ansi. Run `make clean` when switching.
UI ?= curses
TARGET = led
SRCS = main.c model/model.c view/view.c view/screen.c frontend/frontend.c frontend/frontend_memory.c controller/controller.c editor.c undo_store.c config/config.c utils/utils.c test/test_helpers.c test/test_controller.c test/test_view.c test/test_autosave.c test/test_safety.c test/test_screen.c test/test_actions.c test/test_fuzz.c
ifeq ($(UI),ansi)
SRCS += frontend/frontend_ansi.c
else
SRCS += frontend/frontend_curses.c
LDFLAGS += -lncursesw
endif
OBJS = $(SRCS:.c=.o)
$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@
clean:
	rm -f $(TARGET) $(OBJS) frontend/frontend_ansi.o frontend/frontend_curses.o
lint:
	splint -weak -bounds -null -compdef -warnposix +matchanyintegral +longintegral $(SRCS)
sanitize: CFLAGS += -fsanitize=address -fsanitize=undefined
sanitize: $(TARGET)

# Useful on Linux and Unix 
install:
	mkdir /usr/local/bin || true
	cp led /usr/local/bin

doc:
	cp led.1 /usr/share/man/man1

.PHONY: clean lint doc install
