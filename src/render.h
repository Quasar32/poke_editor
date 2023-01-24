#ifndef RENDER_H
#define RENDER_H

#include <cglm/cglm.h>
#include <stdint.h>

#define TILE_MAP_LEN 32 

typedef uint8_t tile_row[TILE_MAP_LEN];
typedef tile_row tile_map[TILE_MAP_LEN];

struct v2b {
	uint8_t x;
	uint8_t y;
};

struct tm_shader {
	GLuint prog;
	GLuint vao;
	GLuint vbo;

	GLint proj_loc;
	GLint tex_loc;
	GLint pal_loc;
	GLint scroll_loc;

	tile_map tm;
	struct v2b scroll;

	GLuint tex;
};

extern GLuint g_pal;
extern struct tm_shader g_tms;
extern struct tm_shader g_wms;

void init_gl(void);
void render(void);

#endif
