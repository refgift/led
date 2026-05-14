# Dependencies: ncurses (for terminal UI)
CC = gcc
CFLAGS = -I. -Iview -Itest -Imodel -Icontroller -Iconfig -D_POSIX_C_SOURCE -D_GNU_SOURCE -Wall -Wextra -Wno-unused-parameter -std=c11 
LDFLAGS = -lncurses
TARGET = led
SRCS = main.c model/model.c view/view.c controller/controller.c editor.c config/config.c test/test_controller.c test/test_view.c test/test_autosave.c
OBJS = $(SRCS:.c=.o)
$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@
clean:
	rm -f $(TARGET) $(OBJS)
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
