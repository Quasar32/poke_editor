CC = gcc

CFLAGS = -Wall -g
CFLAGS += -Ilib/cglm/include -Ilib/glad/include -Ilib/glfw/include
CFLAGS += -MT $@ -MMD -MP -MF $(@:.o=.d)

DEPOBJS = lib/glad/src/glad.o lib/cglm/libcglm.a lib/glfw/src/libglfw3.a 

GEN = -G "MinGW Makefiles"

LDFLAGS = -lopengl32 -mwindows -mconsole -g

SRC = $(wildcard src/*.c)
OBJ = $(patsubst src/%.c,obj/%.o,$(SRC))
DEP = $(patsubst src/%.c,obj/%.d,$(SRC))

all: dirs editor 

lib/cglm/libcglm.a:
	cd lib/cglm && \
	cmake . -DCGLM_STATIC=ON && mingw32-make

lib/glad/src/glad.o:
	cd lib/glad && \
	$(CC) -o src/glad.o -Iinclude -c src/glad.c

lib/glfw/src/glfw3.a:
	cd lib/glfw && \
	cmake . && \
	mingw32-make

dirs: 
	mkdir -p ./bin 
	mkdir -p ./obj

obj/%.o: src/%.c
	$(CC) $(CFLAGS) -o $@ -c $<

-include $(DEP)

editor: $(OBJ) $(DEPOBJS)
	$(CC) -o bin/editor $^ $(LDFLAGS)

clean:
	rm -rf bin $(OBJ) $(DEP)
