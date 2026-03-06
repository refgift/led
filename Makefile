CC = gcc
CFLAGS = -Wall -Wextra -std=c11
LDFLAGS = -lncurses
TARGET = led
SRCS = main.c model.c view.c controller.c editor.c config.c
OBJS = $(SRCS:.c=.o)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(TARGET) $(OBJS)

.PHONY: clean