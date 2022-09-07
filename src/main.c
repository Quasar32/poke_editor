#include <stdio.h>
#include <stdlib.h>

#include <windows.h>

#include <glad/glad.h>
#include <glfw/glfw3.h>

#define SCR_WIDTH 800
#define SCR_HEIGHT 600

#define TILE_DATA_SIZE 6144

#define VAP_STRIDE (5 * sizeof(float))
#define VAP_OFFSET(off) ((void *) (off * sizeof(float)))

#define VAPI_POSITION 0
#define VAPI_COORD 1

static GLFWwindow *g_window;
static GLuint g_prog;

static void glfw_error_callback(int code, const char *description)
{
	fprintf(stderr, "glfw error (%d): %s", code, description);
	exit(1);
}

static void glfw_resize_callback(GLFWwindow *window, int width, int height)
{
	glViewport(0, 0, width, height);
}

static void init_glfw(void) 
{
	glfwSetErrorCallback(glfw_error_callback);
	glfwInit();
	atexit(glfwTerminate);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

	g_window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, 
			"poke_editor", NULL, NULL);
	glfwMakeContextCurrent(g_window);
	glfwSetFramebufferSizeCallback(g_window, glfw_resize_callback);
	glfwSwapInterval(1);

	if (!gladLoadGLLoader((GLADloadproc) glfwGetProcAddress)) {
		fprintf(stderr, "glad error\n");
		exit(1);
	}
}

static void crt_print(const char *msg)
{
	fprintf(stderr, "crt error: %s: %s (%d)\n", 
			msg, strerror(errno), errno);
	exit(1);
}

static void crt_printf(const char *fmt, ...)
{
	va_list ap;
	char msg[1024];

	va_start(ap, fmt);
	vsprintf_s(msg, sizeof(msg), fmt, ap);
	va_end(ap);
	crt_print(msg);
}	

static FILE *xfopen(const char *path, const char *mode)
{
	FILE *f;
	f = fopen(path, mode);
	if (!f) {
		crt_printf("xfopen(%s, %s)", path, mode);
	}
	return f;
}

static void xfseek(FILE *f, long off, int origin)
{
	if (fseek(f, off, origin) != 0) {
		crt_print("xfseek");
	}
}

static long xftell(FILE *f) 
{
	long pos;
	pos = ftell(f);
	if (pos == -1L) {
		crt_print("xftell");
	}
	return pos;
}

static void xfread_obj(FILE *f, void *buf, size_t size)
{
	if (fread(buf, size, 1, f) != 1) {
		crt_print("xfread_obj fail");
	}
}

static int xfgetc(FILE *f)
{
	int ch;
	ch = fgetc(f);
	if (ch == EOF) {
		crt_print("xfgetc fail");
	}
	return ch;
}

static void *xmalloc(size_t size)
{
	void *ptr;
	
	if (size == 0) {
		size = 1;
	}

	ptr = malloc(size);
	if (!ptr) {
		fprintf(stderr, "xmalloc fail");
		exit(1);
	}
	return ptr;
}

static long get_file_size(FILE *f)
{
	long pos;
	xfseek(f, 0, SEEK_END);
	pos = xftell(f);
	xfseek(f, 0, SEEK_SET);
	return pos;
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

static int get_px(int px, int shift)
{
	return ((px >> shift) & 3) << 6;
}

static void decomp_tile(FILE *f, uint8_t *tile) 
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

			c = xfgetc(f);
			b[0] = get_px(c, 6);
			b[1] = get_px(c, 4);
			b[2] = get_px(c, 2);
			b[3] = get_px(c, 0);
			b += 4;
		}
		t += 128;
	}
}

static uint8_t *read_tile_data(const char *path)
{
	uint8_t *tile_data;
	FILE *f;
	uint8_t *r;
	int i;

	tile_data = xmalloc(128 * 128);

	f = xfopen(path, "rb");

	r = tile_data;
	i = 6;
	while (i-- > 0) {
		uint8_t *t;
		int j;
		t = r;
		j = 16;
		while (j-- > 0) {
			decomp_tile(f, t);
			t += 8;
		}
		r += 128 * 8;
	}
	fclose(f);
	return tile_data;
}

static GLuint load_tile_data(const char *path)
{
	GLuint tex;
	uint8_t *tile_data;

	glGenTextures(1, &tex);
	glBindTexture(GL_TEXTURE_2D, tex);

  	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	tile_data = read_tile_data(path);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, 
			128, 128, 0, GL_RED, 
			GL_UNSIGNED_BYTE, tile_data);
	free(tile_data);

	return tex;
}

static GLuint load_pallete(void)
{
	static const uint8_t pallete[] = {
        	0xFF, 0xEF, 0xFF,
        	0xA8, 0xA8, 0xA8,
        	0x80, 0x80, 0x80,
        	0x10, 0x10, 0x18
	};

	GLuint pal; 

	glGenTextures(1, &pal);
	glBindTexture(GL_TEXTURE_1D, pal);

  	glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    	glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_WRAP_T, GL_REPEAT);

	glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	glTexImage1D(GL_TEXTURE_1D, 0, GL_RGB, 4, 0, 
			GL_RGB, GL_UNSIGNED_BYTE, pallete);

	return pal;
}

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

static GLuint compile_shader(GLenum type, const char *path) 
{
	GLuint shader;	
	const char *src;
	GLint success;

	shader = glCreateShader(type);

	src = fread_all_str(path);
	glShaderSource(shader, 1, &src, NULL); 
	free((char *) src);

	glCompileShader(shader);
	glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
	if (!success) {
		prog_printf("\"%s\" compilation failed", shader, path);
	}

	return shader;
}

static GLuint link_shaders(int vs, int fs)
{
	GLuint prog;
	GLint success;

	prog = glCreateProgram();
	glAttachShader(prog, vs);
	glAttachShader(prog, fs);
	glLinkProgram(prog);
	glGetProgramiv(prog, GL_LINK_STATUS, &success);
	if (!success) {
		prog_print("program link failed", prog);
	}
	glDeleteShader(vs);
	glDeleteShader(fs);
	return prog;
}

static void create_program(void) 
{
	GLuint vs;
	GLuint fs;

	vs = compile_shader(GL_VERTEX_SHADER, "res/shader/vertex.vp");
	fs = compile_shader(GL_FRAGMENT_SHADER, "res/shader/fragment.fp");
	g_prog = link_shaders(vs, fs);
}

static void cd_parent(char *path)
{
	char *find;

	find = strrchr(path, '\\');
	if (find) {
		*find = '\0';
	}
}

static void set_default_directory(void)
{
	char path[MAX_PATH];

	GetModuleFileName(NULL, path, MAX_PATH);
	cd_parent(path);
	cd_parent(path);
	SetCurrentDirectory(path);
}

static void add_attrib(GLuint i, GLint size, GLsizei off)
{
	glVertexAttribPointer(i, size, GL_FLOAT,
			GL_FALSE, VAP_STRIDE, VAP_OFFSET(off));
	glEnableVertexAttribArray(i);
}

int main(void) 
{
	static const GLfloat vertices[] = {
		-0.5F, -0.5F, -0.5F,  0.0F,  1.0F,
		 0.5F, -0.5F, -0.5F,  1.0F,  1.0F,
		 0.5F,  0.5F, -0.5F,  1.0F,  0.0F,
		-0.5F,  0.5F, -0.5F,  0.0F,  0.0F
	};

	static const GLuint indices[] = {
		0, 1, 2,
		2, 3, 0
	};

	GLuint vao, vbo, ebo;
	GLuint tex, pal;
	GLuint tex_loc, pal_loc;

	set_default_directory();
	init_glfw();

	create_program();

	glGenVertexArrays(1, &vao);
	glGenBuffers(1, &vbo);
	glGenBuffers(1, &ebo);

	glBindVertexArray(vao);

	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), 
			vertices, GL_STATIC_DRAW);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices),
			indices, GL_STATIC_DRAW);

	add_attrib(VAPI_POSITION, 3, 0);
	add_attrib(VAPI_COORD, 2, 3);

	glUseProgram(g_prog);

	tex = load_tile_data("../Poke/Shared/Tiles/TileData00");
	pal = load_pallete();

	tex_loc = glGetUniformLocation(g_prog, "tex");
	pal_loc = glGetUniformLocation(g_prog, "pal");
	while (!glfwWindowShouldClose(g_window)) {

		glClearColor(0.2F, 0.3F, 0.3F, 1.0F);
		glClear(GL_COLOR_BUFFER_BIT);

		glUseProgram(g_prog);

		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, tex);
		glUniform1i(tex_loc, 0);

		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_1D, pal);
		glUniform1i(pal_loc, 1);

        	glBindVertexArray(vao);
        	glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, NULL);

		glfwSwapBuffers(g_window);
		glfwPollEvents();
	}
	return 0;
}
