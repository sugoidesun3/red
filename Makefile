CC = gcc
#CFLAGS = -std=gnu2x -Wall -Wextra -Werror -pedantic
CFLAGS = -g -std=gnu99 -Wall -Wextra -pedantic -DDEBUG
# -Werror
SRC = $(shell find . -type f -name '*.c')
OBJ = $(SRC:.c=.o)
LIBS = -I ./include/ -lssl -lcrypto
TARGET = client-bckp

.PHONY: all clean purge

all: $(TARGET) clean

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

%.o: %.c
	$(CC) $(CFLAGS) -c $^ -o $@ $(LIBS)

purge: clean
	rm -rf $(TARGET)
clean:
	rm -rf $(OBJ)
