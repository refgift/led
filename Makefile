CC = gcc
CFLAGS = -Wall -Wextra -std=c11
LDFLAGS = -lncurses -lrx
TARGET = led
SRCS = main.c model.c view.c controller.c editor.c config.c test_controller.c
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
