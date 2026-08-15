CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -g
TARGET = mips_sim

SRCS = main.c mips_sim.c unit_tests.c
OBJS = $(SRCS:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all clean