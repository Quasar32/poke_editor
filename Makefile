CC = gcc

CFLAGS = -Wall -g
CFLAGS = -Ilib/cglm/include -Ilib/glad/include -Ilib/glfw/include
CFLAGS += -MT $@ -MMD -MP -MF $*.d

LDFLAGS = -lopengl32 -mwindows -mconsole -g
LDFLAGS += lib/glad/src/glad.o lib/cglm/libcglm.a lib/glfw/src/libglfw3.a 

SRC = $(wildcard src/*.c) 
OBJ = $(SRC:.c=.o)
DEP = $(SRC:%.c=%.d)

all: libs dirs editor 

libs:
	if not exist lib/cglm/libcglm.a \
		cd lib/cglm && cmake . -DCGLM_STATIC=ON && mingw32-make
	if not exist lib/glad/src/glad.o \
		cd lib/glad && $(CC) -o src/glad.o -Iinclude -c src/glad.c
	if not exist lib/glfw/src/libglfw3.a \
		cd lib/glfw && cmake . && mingw32-make
dirs: 
	if not exist bin \
		mkdir bin 

%.o: %.c %.d
	$(CC) $(CFLAGS) -o $@ -c $<

include $(wildcard $(DEP))

editor: $(OBJ)
	$(CC) -o bin/editor $^ $(LDFLAGS)

clean:
	rm -rf bin $(OBJ) $(DEP)
