CC      = gcc
CFLAGS  = -Wall -Wextra -g -O2 $(shell pkg-config --cflags sdl2 SDL2_ttf)
LIBS    = -lpthread -lm $(shell pkg-config --libs sdl2 SDL2_ttf)

TARGET  = bridge

SRCS    = main.c        \
          config.c      \
          bridge.c      \
          vehicle.c     \
          mode_carnage.c  \
          mode_semaphore.c \
          mode_officer.c  \
          gui.c

OBJS    = $(SRCS:.c=.o)

.PHONY: all clean run1 run2 run3 install-deps

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@


install-deps:
	sudo apt-get install -y libsdl2-dev libsdl2-ttf-dev fonts-dejavu


run1: $(TARGET)
	./$(TARGET) prueba 1

run2: $(TARGET)
	./$(TARGET) prueba 2

run3: $(TARGET)
	./$(TARGET) prueba 3

clean:
	rm -f $(OBJS) $(TARGET)
