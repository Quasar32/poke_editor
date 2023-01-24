#include <stdarg.h>
#include <string.h>

#include "render.h"
#include "xstd.h"

GLuint g_pal;
struct tm_shader g_tms;
struct tm_shader g_wms;

static void prog_print(const char *msg, int prog) 
{
	char err[1024];
	glGetProgramInfoLog(prog, sizeof(err), NULL, err); 
	fprintf(stderr, "gl error: %s: %s\n", msg, err);
}

static void prog_printf(const char *fmt, int prog, ...) 
{
	va_list ap;
	char msg[1024];

	va_start(ap, prog);
	vsprintf_s(msg, sizeof(msg), fmt, ap);
	va_end(ap);
	prog_print(msg, prog);
}

static GLuint link_shaders(GLuint vs, GLuint gs, GLuint fs)
{
	GLuint prog;
	GLint success;

	prog = glCreateProgram();

	glAttachShader(prog, vs);
	glAttachShader(prog, gs);
	glAttachShader(prog, fs);

	glLinkProgram(prog);
	glGetProgramiv(prog, GL_LINK_STATUS, &success);
	if (!success) {
		prog_print("program link failed", prog);
	}

	glDetachShader(prog, fs);
	glDetachShader(prog, gs);
	glDetachShader(prog, vs);
	glDeleteShader(fs);
	glDeleteShader(gs);
	glDeleteShader(vs);

	return prog;
}

static void pad_tile(uint8_t *tile)
{
	uint8_t *t;
	int i;

	t = tile;
	i = 8;
	while (i-- > 0) {
		uint8_t *b;
		int j;

		b = t;
		j = 2;
		while (j-- > 0) {
			memset(b, 0, 4);
			b += 4;
		}
		t += 128;
	}
}

static int get_px(int px, int shift)
{
	return ((px >> shift) & 3) << 6;
}

static int decomp_tile(FILE *f, uint8_t *tile) 
{
	uint8_t *t;
	int i;

	t = tile;
	i = 8;
	while (i-- > 0) {
		uint8_t *b;
		int j;

		b = t;
		j = 2;
		while (j-- > 0) {
			int c;

			c = fgetc(f);
			if (c == EOF) {
				return -1;
			}

			b[0] = get_px(c, 6);
			b[1] = get_px(c, 4);
			b[2] = get_px(c, 2);
			b[3] = get_px(c, 0);
			b += 4;
		}
		t += 128;
	}
	return 0;
}

static uint8_t *read_tile_data(const char *path, int pad)
{
	uint8_t *tile_data;
	FILE *f;
	uint8_t *r;
	int i;

	tile_data = xmalloc(128 * 128);

	f = xfopen(path, "rb");

	r = tile_data;

	i = 16;
	while (i-- > 0) {
		uint8_t *t;
		int j;

		t = r;
		j = 16;
		while (j-- > 0) {
			if (pad > 0) {
				pad_tile(t);
				pad--;
			} else if (decomp_tile(f, t) < 0) {
				goto end;
			}
			t += 8;
		}
		r += 128 * 8;
	}
end:
	fclose(f);
	return tile_data;
}

static char *fread_all_str(const char *path)
{
	FILE *f;
	long len;
	char *buf;

	f = xfopen(path, "rb");
	len = get_file_size(f);
	buf = xmalloc(len + 1);
	xfread_obj(f, buf, len);
	buf[len] = '\0';
	fclose(f);
	return buf;
}

static GLuint load_tile_data(const char *path, int pad)
{
	GLuint tex;
	uint8_t *tile_data;

	glGenTextures(1, &tex);
	glBindTexture(GL_TEXTURE_2D, tex);

  	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
			GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	tile_data = read_tile_data(path, pad);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, 
			128, 128, 0, GL_RED, 
			GL_UNSIGNED_BYTE, tile_data);
	free(tile_data);

	return tex;
}

static void load_pallete(void)
{
	static const uint8_t pallete[] = {
        	0xFF, 0xEF, 0xFF,
        	0xA8, 0xA8, 0xA8,
        	0x80, 0x80, 0x80,
        	0x10, 0x10, 0x18
	};

	glGenTextures(1, &g_pal);
	glBindTexture(GL_TEXTURE_1D, g_pal);

  	glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    	glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	glTexImage1D(GL_TEXTURE_1D, 0, GL_RGB, 4, 0, 
			GL_RGB, GL_UNSIGNED_BYTE, pallete);
	/*
	glBindTexture(GL_TEXTURE_1D, g_pal);
	glTexSubImage1D(GL_TEXTURE_1D, 0, 0, 4, GL_RGB, 
			GL_UNSIGNED_BYTE, pallete);
	*/
}

static GLuint compile_shader(GLenum type, const char *path) 
{
	GLuint shader;
	char *src;
	GLint success;

	shader = glCreateShader(type);

	src = fread_all_str(path);
	glShaderSource(shader, 1, (const char **) &src, NULL); 
	free(src);

	glCompileShader(shader);
	glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
	if (!success) {
		prog_printf("\"%s\" compilation failed", shader, path);
	}

	return shader;
}

static void load_tms(struct tm_shader *tms, const char *gs_path)
{
	static vec3 flip = {-1.0F, 1.0F, 0.0F};
	static vec3 scale = {0.1F, -1.0/9.0F, 1.0F};

	GLuint vs;
	GLuint gs;
	GLuint fs;
	mat4 projection;
	
	vs = compile_shader(GL_VERTEX_SHADER, "res/shaders/tm.vert");
	gs = compile_shader(GL_GEOMETRY_SHADER, gs_path);
	fs = compile_shader(GL_FRAGMENT_SHADER, "res/shaders/tm.frag");
	tms->prog = link_shaders(vs, gs, fs);

	glGenVertexArrays(1, &tms->vao);
	glGenBuffers(1, &tms->vbo);

	glBindVertexArray(tms->vao);

	glBindBuffer(GL_ARRAY_BUFFER, tms->vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(tms->tm), 
			tms->tm, GL_DYNAMIC_DRAW);

	glVertexAttribIPointer(0, 1, GL_UNSIGNED_BYTE, 1, NULL);
	glEnableVertexAttribArray(0);

	glUseProgram(tms->prog);

	tms->proj_loc = glGetUniformLocation(tms->prog, "projection");
	tms->scroll_loc = glGetUniformLocation(tms->prog, "scroll");
	tms->tex_loc = glGetUniformLocation(tms->prog, "tex");
	tms->pal_loc = glGetUniformLocation(tms->prog, "pal");

	glUniform1i(tms->tex_loc, 0);
	glUniform1i(tms->pal_loc, 1);

	glm_mat4_identity(projection);
	glm_translate(projection, flip); 
	glm_scale(projection, scale);
	glUniformMatrix4fv(tms->proj_loc, 1, GL_FALSE, (float *) projection); 
}

void init_gl(void)
{
	load_pallete();

	load_tms(&g_tms, "res/shaders/tm.geom");
	load_tms(&g_wms, "res/shaders/wm.geom");
	g_tms.tex = load_tile_data("../Poke/Shared/Tiles/TileData00", 0);
	g_wms.tex = load_tile_data("../Poke/Shared/Tiles/TileDataMenu", 2);
}

static void render_tms(struct tm_shader *tms)
{
	vec2 scroll;

	glUseProgram(tms->prog);
	
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_1D, g_pal);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, tms->tex);

	scroll[0] = tms->scroll.x / 8.0F - 1.0F;
	scroll[1] = tms->scroll.y / 8.0F - 1.0F;
	glUniform2f(tms->scroll_loc, scroll[0], scroll[1]); 

	glBindVertexArray(tms->vao);

	glBindBuffer(GL_ARRAY_BUFFER, tms->vbo);
	glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(tms->tm), tms->tm);

	glVertexAttribIPointer(0, 1, GL_UNSIGNED_BYTE, 1, NULL);
	glEnableVertexAttribArray(0);

        glDrawArrays(GL_POINTS, 0, sizeof(tms->tm));
}

void render(void)
{
	glClearColor(0.2F, 0.3F, 0.3F, 1.0F);
	glClear(GL_COLOR_BUFFER_BIT);

	render_tms(&g_tms);
	render_tms(&g_wms);
}
