# Dependencies: ncurses (for terminal UI), rx (regex library for search functionality)
CC = gcc
CFLAGS = -D_POSIX_C_SOURCE -Wall -Wextra -std=c11 -g
LDFLAGS = -lncurses
TARGET = led
SRCS = main.c model.c view.c controller.c editor.c config.c test_controller.c test_view.c
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

.PHONY: clean lint
