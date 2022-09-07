CC = gcc
CFLAGS = -Wall -g
CFLAGS = -Ilib/glad/include -Ilib/glfw/include 
LDFLAGS = -lopengl32 -mwindows -mconsole
LDFLAGS += lib/glad/src/glad.o lib/glfw/src/libglfw3.a 

SRC = $(wildcard src/*.c) 
OBJ = $(SRC:.c=.o)
BIN = bin

all: libs dirs editor 

dirs:
	mkdir $(BIN)

libs:
	cd lib/glad && $(CC) -o src/glad.o -Iinclude -c src/glad.c
	cd lib/glfw && cmake . && mingw32-make

editor: $(OBJ)
	$(CC) -o $(BIN)/editor $^ $(LDFLAGS)

%.o: %.c
	$(CC) -o $@ -c $< $(CFLAGS)

clean:
	rm -rf $(BIN) $(OBJ)
