#include <stdio.h>
#include <stdlib.h>

#include <windows.h>

#include <cglm/cglm.h>
#include <glad/glad.h>
#include <glfw/glfw3.h>

#include "dl.h"
#include "sds.h"
#include "xmalloc.h"

#define WND_WIDTH 640 
#define WND_HEIGHT 576 

#define TILE_DATA_SIZE 6144

#define TILE_MAP_LEN 32 
#define SIZEOF_TILE_MAP (TILE_MAP_LEN * TILE_MAP_LEN) 

#define MIN(a, b) ((a) < (b) ? (a) : (b))

typedef uint8_t tile_row[TILE_MAP_LEN];
typedef tile_row tile_map[TILE_MAP_LEN];
typedef uint8_t map_row[256];

enum menu_tile {
	MT_EMPTY = 0,
	MT_BLANK = 1,
	MT_TOP_LEFT = 2,
	MT_MIDDLE = 3,
	MT_TOP_RIGHT = 4,
	MT_CENTER_LEFT = 5,
	MT_BOTTOM_LEFT = 6,
	MT_CENTER_RIGHT = 7,
	MT_BOTTOM_RIGHT = 8,
	MT_ZERO = 9,
	MT_NINE = 18,
	MT_COLON = 19,
	MT_CAPITAL_A = 20,
	MT_CAPITAL_U = 40,
	MT_CAPITAL_Z = 45,
	MT_LOWERCASE_A = 46,
	MT_LOWERCASE_L = 57,
	MT_LOWERCASE_Z = 71,
	MT_ACCENTED_E = 72,
	MT_EXCLAMATION_POINT = 74,
	MT_QUOTE_S = 75,
	MT_DASH = 76,
	MT_FULL_VERT_ARROW = 77,
	MT_QUOTE_M = 78,
	MT_COMMA = 79,
	MT_FULL_HORZ_ARROW = 80,
	MT_PERIOD = 81,
	MT_EMPTY_HORZ_ARROW = 82,
	MT_SEMI_COLON = 83,
	MT_LEFT_BRACKET = 84,
	MT_RIGHT_BRACKET = 85,
	MT_LEFT_PARENTHESIS = 86,
	MT_RIGHT_PARENTHESIS = 87,
	MT_MALE_SYMBOL = 88,
	MT_FEMALE_SYMBOL = 89,
	MT_SLASH = 90,
	MT_PK = 91,
	MT_MN = 92,
	MT_MIDDLE_SCORE = 93,
	MT_UPPER_SCORE = 94,
	MT_END = 95,
	MT_TIMES = 96,
	MT_QUESTION = 97,
	MT_TRAINER = 98,
	MT_TRANSITION = 147,
	MT_MAP = 171,
	MT_MAP_BRACKET = 183
};

enum quad_props {
	QP_NONE,
	QP_SOLID,
	QP_EDGE,
	QP_MSG,
	QP_WATER,	
	QP_DOOR,
	QP_EXIT,
	QP_TV,
	QP_SHELF,
	QP_PC,
	QP_MAP
};

struct v2b {
	uint8_t x;
	uint8_t y;
};

struct sel {
	struct v2b pos;
	struct v2b dpos;
	struct v2b tl;
	struct v2b br;
	int blank;
};

struct text {
	struct dl_head node;
	struct v2b pos;
	sds str;
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

static const char g_prop_strs[][6] = { 
	"None ",
	"Solid",
	"Edge ",
	"Msg  ",
	"Water",
	"Door ",  
	"Exit ",
	"TV   ",
	"Shelf",
	"PC   ",
	"Map  "
};

struct tm_shader g_tms;
struct tm_shader g_wms;

static GLuint g_pal;

static ivec2 g_cam;

static int g_qm_width;
static int g_qm_height;
static map_row g_qm_data[256];
static int g_def_quad;

static size_t g_text_count;
static DL_HEAD(g_texts);

static GLFWwindow *g_wnd;
static int g_vx;
static int g_vy;
static int g_vw = WND_WIDTH;
static int g_vh = WND_HEIGHT;

static tile_map g_window_map; 

static uint8_t g_quad_data[128][2][2];
static uint8_t g_quad_props[128];

static int g_place;
static int g_qsel_page = 0;
static struct sel g_qsel = {
	.pos = {2, 3},
	.dpos = {3, 3},
	.tl = {2, 3},
	.br = {14, 12},
	.blank = MT_BLANK
};

static struct sel g_qm_sel = {
	.dpos = {2, 2},
	.blank = MT_EMPTY
};

static struct sel g_qtsel = {
	.dpos = {2, 2},
	.tl = {13, 3},
	.br = {15, 5},
	.blank = MT_BLANK
};

static int g_tsel_page;
static struct sel g_tsel = {
	.dpos = {2, 2},
	.tl = {2, 7},
	.br = {16, 13},
	.blank = MT_BLANK
};

static void error_cb(int code, const char *description)
{
	fprintf(stderr, "glfw error (%d): %s", code, description);
	exit(1);
}

static void resize_cb(GLFWwindow *wnd, int w, int h)
{
	int vc, vr;

	vc = w * WND_HEIGHT;
	vr = h * WND_WIDTH;
	if (vc > vr) {
		g_vw = vr / WND_HEIGHT;
		g_vh = h;
	} else {
		g_vw = w;
		g_vh = vc / WND_WIDTH;
	}

	g_vx = (w - g_vw) / 2;
	g_vy = (h - g_vh) / 2;

	glViewport(g_vx, g_vy, g_vw, g_vh);
}

static int q_in_bounds(int qx, int qy)
{
	return qx >= 0 && qx < g_qm_width && qy >= 0 && qy < g_qm_height;
}

static void q_to_t(int tx, int ty, int d)
{
	uint8_t (*q)[2];
	int tx0, ty0;
	int tx1, ty1;

	q = g_quad_data[d];
	tx0 = tx & 31;
	ty0 = ty & 31;
	tx1 = (tx + 1) & 31;
	ty1 = (ty + 1) & 31;

	g_tms.tm[ty0][tx0] = q[0][0];
	g_tms.tm[ty0][tx1] = q[0][1];
	g_tms.tm[ty1][tx0] = q[1][0];
	g_tms.tm[ty1][tx1] = q[1][1];
}

static void mq_to_t(int tx, int ty, int qx, int qy)
{
	int d;
	
	d = q_in_bounds(qx, qy) ? g_qm_data[qy][qx] : g_def_quad;
	q_to_t(tx, ty, d);
}

static void qm_to_tm(ivec2 t, ivec4 q)
{
	int ty;
	int qy;

	ty = t[1];
	for (qy = q[1]; qy < q[3]; qy++) {
		int tx;
		int qx;

		tx = t[0];
		for (qx = q[0]; qx < q[2]; qx++) {
			mq_to_t(tx, ty, qx, qy);
			tx += 2;
			tx &= 31;
		}
		ty += 2;
		ty &= 31;
	}
}

static void move_cam_left(void)
{
	if (g_cam[0] > 0) {
		ivec2 t;
		ivec4 q;

		g_cam[0]--;
		g_tms.scroll.x -= 16;

		t[0] = (g_tms.scroll.x / 8) & 31;
		t[1] = (g_tms.scroll.y / 8) & 31;

		q[0] = g_cam[0];
		q[1] = g_cam[1];
		q[2] = g_cam[0] + 1;
		q[3] = g_cam[1] + 9;

		qm_to_tm(t, q);
	}
}

static void move_cam_right(void)
{
	if (g_cam[0] < g_qm_width - 10) {
		ivec2 t;
		ivec4 q;

		g_cam[0]++;
		g_tms.scroll.x += 16;

		t[0] = (g_tms.scroll.x / 8 + 18) & 31;
		t[1] = (g_tms.scroll.y / 8) & 31;

		q[0] = g_cam[0] + 9;
		q[1] = g_cam[1]; 
		q[2] = g_cam[0] + 10;
		q[3] = g_cam[1] + 9;

		qm_to_tm(t, q);
	}
}

static void move_cam_down(void)
{
	if (g_cam[1] < g_qm_height - 9) {
		ivec2 t;
		ivec4 q;

		g_cam[1]++;
		g_tms.scroll.y += 16;

		t[0] = (g_tms.scroll.x / 8) & 31;
		t[1] = (g_tms.scroll.y / 8 + 16) & 31;

		q[0] = g_cam[0];
		q[1] = g_cam[1] + 8; 
		q[2] = g_cam[0] + 10;
		q[3] = g_cam[1] + 9;

		qm_to_tm(t, q);
	}
}

static void move_cam_up(void)
{
	if (g_cam[1] > 0) {
		ivec2 t;
		ivec4 q;

		g_cam[1]--;
		g_tms.scroll.y -= 16;

		t[0] = (g_tms.scroll.x / 8) & 31;
		t[1] = (g_tms.scroll.y / 8) & 31;

		q[0] = g_cam[0];
		q[1] = g_cam[1]; 
		q[2] = g_cam[0] + 10;
		q[3] = g_cam[1] + 1;

		qm_to_tm(t, q);
	}
}

static void open_qsel();

static int wrap(int v, int l, int h)
{
	return v < l ? h : (v > h ? l : v); 
}

static void rel_sel(struct sel *s, struct v2b *rel)
{
	rel->x = (s->pos.x - s->tl.x) / s->dpos.x;
	rel->y = (s->pos.y - s->tl.y) / s->dpos.y;
}

static void place_sel(struct sel *s)
{
	g_wms.tm[s->pos.y][s->pos.x] = MT_FULL_HORZ_ARROW;
}

static void remove_sel(struct sel *s)
{
	g_wms.tm[s->pos.y][s->pos.x] = s->blank;
}

static void move_sel(struct sel *s, int dx, int dy) 
{
	remove_sel(s);
	s->pos.x = wrap(s->pos.x + dx, s->tl.x, s->br.x);
	s->pos.y = wrap(s->pos.y + dy, s->tl.y, s->br.y);
	place_sel(s);
}

static int move_sel_kb(int key, struct sel *s) 
{
	switch (key) {
	case GLFW_KEY_A:
		move_sel(s, -s->dpos.x, 0);
		return 1;
	case GLFW_KEY_D:
		move_sel(s, s->dpos.x, 0);
		return 1;
	case GLFW_KEY_S:
		move_sel(s, 0, s->dpos.y);
		return 1;
	case GLFW_KEY_W:
		move_sel(s, 0, -s->dpos.y);
		return 1;
	}
	return 0;
}

static void place_quad(void)
{	
	int tx, ty;
	int qx, qy;

	tx = g_qm_sel.pos.x + g_tms.scroll.x / 8; 
	ty = g_qm_sel.pos.y + g_tms.scroll.y / 8; 

	qx = g_cam[0] + g_qm_sel.pos.x / 2;
	qy = g_cam[1] + g_qm_sel.pos.y / 2;

	g_qm_data[qy][qx] = g_place;  

	mq_to_t(tx, ty, qx, qy);
}

static int g_is_x_held;

static int ch_to_tile(int ch) 
{
    switch(ch) {
    case ' ':
        return MT_BLANK;
    case '0' ... '9':
        return MT_ZERO + (ch - '0');
    case ':':
        return MT_COLON;
    case 'A' ... 'Z':
        return MT_CAPITAL_A + (ch - 'A');
    case 'a' ... 'z':
        return MT_LOWERCASE_A + (ch - 'a');
    case '\xE9':
        return MT_ACCENTED_E;
    case '!':
        return MT_EXCLAMATION_POINT;
    case '\'':
        return MT_QUOTE_S;
    case '-':
        return MT_DASH;
    case '~':
        return MT_QUOTE_M;
    case ',':
        return MT_COMMA;
    case '.':
        return MT_PERIOD;
    case ';':
        return MT_SEMI_COLON;
    case '[':
        return MT_LEFT_BRACKET;
    case ']':
        return MT_RIGHT_BRACKET; 
    case '(':
        return MT_LEFT_PARENTHESIS;
    case ')':
        return MT_RIGHT_PARENTHESIS;
    case '^':
        return MT_MALE_SYMBOL;
    case '|':
        return MT_FEMALE_SYMBOL;
    case '/':
        return MT_SLASH;
    case '\1':
        return MT_PK;
    case '\2':
        return MT_MN;
    case '=':
        return MT_MIDDLE_SCORE;
    case '_':
        return MT_UPPER_SCORE;
    case '\3':
        return MT_END;
    case '*': 
        return MT_TIMES;
    case '?':
        return MT_QUESTION;
    }
    return MT_EMPTY;
}

static void place_text(int x0, int y0, const char *text)
{
	const char *t;
	int x;
	int y;

	x = x0;
	y = y0;

	t = text;
	while (*t) {
		switch (*t) {
		case '\n':
			x = x0; 
			y += 2;
			break;
		default:
			g_wms.tm[y][x] = ch_to_tile(*t);
			x++;
		}
		t++;
	}
}

static void place_textf(int x0, int y0, const char *fmt, ...)
{
	va_list ap;
	char buf[360];

	va_start(ap, fmt);
	vsnprintf(buf, 360, fmt, ap);	
	va_end(ap);

	place_text(x0, y0, buf);
}

static void place_row(uint8_t *row, int x0, int x1, int l, int c, int r)
{
	int iw;

	iw = x1 - x0 - 2;
	row[x0] = l;
	memset(row + x0 + 1, c, iw);
	row[x1 - 1] = r;
}

static struct text *add_text(int x, int y, sds s)
{
	struct text *t;

	t = xmalloc(sizeof(*t));

	dl_push_back(&g_texts, &t->node);
	t->pos.x = x;
	t->pos.y = y;
	t->str = s;

	g_text_count++;

	return t;
}

static struct text *get_text(int x, int y)
{
	struct text *t;

	if (g_quad_props[g_qm_data[y][x]] != QP_MSG) {
		return NULL;
	}	

	dl_for_each_entry (t, &g_texts, node) {
		if (t->pos.x == x && t->pos.y == y) {
			return t;
		}
	}


	return add_text(x, y, sdsempty());
}

static void edit_key_cb(GLFWwindow *wnd, int key, 
		int scancode, int action, int mods)
{
	if (action == GLFW_PRESS || action == GLFW_REPEAT) {
		g_qm_sel.br.x = 2 * MIN(9, g_qm_width - g_cam[0] - 1);
		g_qm_sel.br.y = 2 * MIN(8, g_qm_height - g_cam[1] - 1);

		if (!move_sel_kb(key, &g_qm_sel)) {
			switch (key) {
			case GLFW_KEY_X:
				g_is_x_held = 1;
				place_quad();
				break;
			case GLFW_KEY_RIGHT:
				move_cam_right();
				break;
			case GLFW_KEY_LEFT:
				move_cam_left();
				break;
			case GLFW_KEY_DOWN:
				move_cam_down();
				break;
			case GLFW_KEY_UP:
				move_cam_up();
				break;
			case GLFW_KEY_ENTER:
				remove_sel(&g_qm_sel);
				open_qsel();
				break;
			}
		} else if (g_is_x_held) {
			place_quad();
		}
	} else if(key == GLFW_KEY_X && action == GLFW_RELEASE) {
		g_is_x_held = 0;
	}
}

static void open_edit(void)
{
	g_wms.tm[g_qm_sel.pos.y][g_qm_sel.pos.x] = MT_FULL_HORZ_ARROW;
	glfwSetKeyCallback(g_wnd, edit_key_cb);
}

static void get_abs_tpt(struct v2b *out, int rtx, int rty)
{
	out->x = g_tms.scroll.x / 8 + rtx;
	out->y = g_tms.scroll.y / 8 + rty;
}

static void wm_q_to_t(int x, int y, int d)
{
	int sx;
	int sy;

	memset(g_wms.tm[y] + x, 0, 2);
	memset(g_wms.tm[y + 1] + x, 0, 2);

	sx = g_tms.scroll.x / 8 + x;
	sy = g_tms.scroll.y / 8 + y;
	q_to_t(sx, sy, d);
}

static void place_box(int x0, int y0, int x1, int y1)
{
	tile_row *row;
	int iw;
	int n;

	row = g_wms.tm + y0;
	place_row(*row, x0, x1, MT_TOP_LEFT, MT_MIDDLE, MT_TOP_RIGHT);

	n = y1 - y0 - 2;
	while (n-- > 0) {
		row++;
		place_row(*row, x0, x1, MT_CENTER_LEFT, 
				MT_BLANK, MT_CENTER_RIGHT);
	}

	place_row(*row, x0, x1, MT_BOTTOM_LEFT, MT_MIDDLE, MT_BOTTOM_RIGHT);
}

static void place_t_on_wm(int tx, int ty, int tile)
{
	struct v2b av;

	g_wms.tm[ty][tx] = MT_EMPTY;
	get_abs_tpt(&av, tx, ty);
	g_tms.tm[av.y][av.x] = tile;
}

static void open_qtsel(void);
static void qtsel_key_cb(GLFWwindow *wnd, int key, 
		int scancode, int action, int mode);

static void edit_quad(void)
{
	struct v2b tv;
	struct v2b qv;
	int t;
	uint8_t *q;
	uint8_t *tp;

	rel_sel(&g_tsel, &tv);
	rel_sel(&g_qsel, &qv);
	t = tv.x + tv.y * 8 + g_tsel_page * 24;
	q = g_quad_data[g_place][qv.y] + qv.x;
	*q = t;
	g_tms.tm[g_qtsel.pos.y][g_qtsel.pos.x + 1] = t;
	g_tms.tm[qv.y + 3][qv.x + 3] = t;
}

static void mod_tsel(void)
{
	int tx, ty;
	int t;
	int n;

	t = g_tsel_page * 24;
	ty = 5;

	n = 32;
	while (n-- > 0) {
		struct v2b av;

		if (t % 8 == 0) {
			tx = 3;
			ty += 2;
		}

		get_abs_tpt(&av, tx, ty);

		g_wms.tm[ty][tx] = MT_EMPTY;
		g_tms.tm[av.y][av.x] = t++;

		tx += 2;
	}
	place_textf(15, 15, "%d/3", g_tsel_page + 1);
}

static void place_prop(void)
{
	int prop;
	const char *str;

	prop = g_quad_props[g_place];
	if (prop < _countof(g_prop_strs)) {
		str = g_prop_strs[prop];	
	} else {
		str = "TBD  ";
	}
	place_textf(6, 3, "%s\n%3d", str, prop);
}

static void tsel_key_cb(GLFWwindow *wnd, int key, 
		int scancode, int action, int mode)
{
	if (action == GLFW_PRESS && !move_sel_kb(key, &g_tsel)) { 
		switch (key) {
		case GLFW_KEY_X:
			edit_quad();
			break;
		case GLFW_KEY_Z:
			glfwSetKeyCallback(g_wnd, qtsel_key_cb); 
			break;
		case GLFW_KEY_RIGHT:
			g_tsel_page = wrap(g_tsel_page + 1, 0, 2);
			mod_tsel();
			break;
		case GLFW_KEY_LEFT:
			g_tsel_page = wrap(g_tsel_page - 1, 0, 2);
			mod_tsel();
			break;
		}
	}
}

static void open_tsel(void)
{
	glfwSetKeyCallback(g_wnd, tsel_key_cb); 
}

static void qtsel_key_cb(GLFWwindow *wnd, int key, 
		int scancode, int action, int mode)
{
	static ivec4 rc = {13, 3, 15, 5}; 

	if (action == GLFW_PRESS && !move_sel_kb(key, &g_qtsel)) { 
		switch (key) {
		case GLFW_KEY_X:
			open_tsel();
			break;
		case GLFW_KEY_Z:
			open_qsel();
			break;
		case GLFW_KEY_UP:
			g_quad_props[g_place]++;
			place_prop();	
			break;
		case GLFW_KEY_DOWN:
			g_quad_props[g_place]--;
			place_prop();	
			break;
		}
	}
}

static void open_qtsel(void)
{
	uint8_t (*q)[2];
 
	place_box(1, 1, 19, 18); 

	wm_q_to_t(3, 3, g_place);

	q = g_quad_data[g_place];
	place_t_on_wm(14, 3, q[0][0]); 
	place_t_on_wm(16, 3, q[0][1]); 
	place_t_on_wm(14, 5, q[1][0]); 
	place_t_on_wm(16, 5, q[1][1]); 

	g_qtsel.pos.x = 13;
	g_qtsel.pos.y = 3;
	g_wms.tm[g_qtsel.pos.y][g_qtsel.pos.x] = MT_FULL_HORZ_ARROW;

	g_tsel.pos.x = 2;
	g_tsel.pos.y = 7;
	g_tsel_page = 0;
	g_wms.tm[g_tsel.pos.y][g_tsel.pos.x] = MT_FULL_HORZ_ARROW;

	place_prop();	

	mod_tsel();
	glfwSetKeyCallback(g_wnd, qtsel_key_cb); 
}

static void mod_qsel(void)
{
	int x;
	int y;
	int t;

	x = 3;
	y = 3;
	t = 0;

	while (t < 20) {
		wm_q_to_t(x, y, t + g_qsel_page * 20); 
		if (x < 15) {
			x += 3;
		} else {
			x = 3;
			y += 3;
		}
		t++;
	}
	place_textf(15, 15, "%d/6", g_qsel_page + 1);
}

static void update_place(void)
{
	ivec2 off;

	off[0] = (g_qsel.pos.x - 2) / 3;
	off[1] = (g_qsel.pos.y - 3) / 3; 
	g_place = g_qsel_page * 20 + off[0] + off[1] * 5; 
}

static void tm_to_qm_screen(void)
{
	ivec4 cam_rc;
	ivec2 scroll;

	scroll[0] = g_tms.scroll.x / 8;
	scroll[1] = g_tms.scroll.y / 8;
	cam_rc[0] = g_cam[0];
	cam_rc[1] = g_cam[1];
	cam_rc[2] = g_cam[0] + 10;
	cam_rc[3] = g_cam[1] + 9;
	qm_to_tm(scroll, cam_rc);
}

static void clear_wm(void)
{
	memset(g_wms.tm, 0, sizeof(g_wms.tm));
}

static void close_qsel(void)
{
	update_place();
	tm_to_qm_screen();
	clear_wm();
	open_edit();
}

static void qsel_key_cb(GLFWwindow *wnd, int key,
		int scancode, int action, int mode)
{

	if (action == GLFW_PRESS && !move_sel_kb(key, &g_qsel)) {
		switch (key) {
		case GLFW_KEY_X: 
			update_place();
			open_qtsel();
			break;
		case GLFW_KEY_Z:
			close_qsel();
			break;
		case GLFW_KEY_RIGHT:
			g_qsel_page = wrap(g_qsel_page + 1, 0, 5);
			mod_qsel();
			break;
		case GLFW_KEY_LEFT:
			g_qsel_page = wrap(g_qsel_page - 1, 0, 5);
			mod_qsel();
			break;
		}
	}
}

static void open_qsel(void)
{
	place_box(1, 1, 19, 18);
	g_wms.tm[g_qsel.pos.y][g_qsel.pos.x] = MT_FULL_HORZ_ARROW;
	mod_qsel();
	glfwSetKeyCallback(g_wnd, qsel_key_cb);
}

static void init_glfw(void) 
{
	glfwSetErrorCallback(error_cb);
	glfwInit();
	atexit(glfwTerminate);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

	g_wnd = glfwCreateWindow(WND_WIDTH, WND_HEIGHT, 
			"poke_editor", NULL, NULL);
	glfwMakeContextCurrent(g_wnd);
	glfwSwapInterval(1);

	glfwSetFramebufferSizeCallback(g_wnd, resize_cb);
	open_edit();

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
	if (fread(buf, size, 1, f) != 1 && size != 0) {
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

static void fread_all_obj(const char *path, void *buf, size_t size)
{
	FILE *f;

	f = xfopen(path, "rb");
	xfread_obj(f, buf, size); 
	fclose(f);
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
			} else if (decomp_tile(f, t) < 0) {
				goto end;
			}
			pad--;
			t += 8;
		}
		r += 128 * 8;
	}
end:
	fclose(f);
	return tile_data;
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

static void decomp_map_quads(FILE *f)
{
	int x, y;
	int n;

	x = 0;
	y = 0;
	n = g_qm_width * g_qm_height;
	while (n > 0) {
		int raw;
		int quad;
		int repeat;

		raw = xfgetc(f);
		quad = raw & 127;
		repeat = 0;

		if (raw == quad) {
			repeat = 1;
		} else {
			repeat = xfgetc(f) + 1; 
		}

		if (repeat > n) {
			fprintf(f, "read_map: quad overflow");
			exit(1);
		}

		n -= repeat;

		while (repeat-- > 0) {
			g_qm_data[y][x] = quad;
			x++;
			if (x >= g_qm_width) {
				x = 0;
				y++;
			}
		}
	}
}

static sds read_pstr(FILE *f)
{
	uint8_t len;
	sds str;

	len = xfgetc(f);
	str = sdsnewlen(SDS_NOINIT, len);
	xfread_obj(f, str, len);
	return str;
}

static void read_texts(FILE *f)
{
	size_t n;

	n = xfgetc(f);

	while (n-- > 0) {
		int x, y;
		sds s;

		x = xfgetc(f);
		y = xfgetc(f);
		s = read_pstr(f);
		add_text(x, y, s);
	}
}

static void read_map(const char *path)
{
	FILE *f;

	f = xfopen(path, "rb");

	g_qm_width = xfgetc(f) + 1;
	g_qm_height = xfgetc(f) + 1;
	
	decomp_map_quads(f);
	g_def_quad = xfgetc(f);
	read_texts(f);

	fclose(f);
}

static void set_up_map(void)
{
	fread_all_obj("../Poke/Shared/Tiles/QuadData00", 
			g_quad_data, sizeof(g_quad_data));
	fread_all_obj("../Poke/Shared/Tiles/QuadProps00",
			g_quad_props, sizeof(g_quad_props));
	read_map("../Poke/Shared/Maps/PalletTown");
	qm_to_tm((ivec2) {0, 0}, (ivec4) {0, 0, 10, 9});
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

static void load_tms(struct tm_shader *tms, const char *gs_path)
{
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
	glm_translate(projection, (vec3) {-1.0F, 1.0F, 0.0F}); 
	glm_scale(projection, (vec3) {1.0F/10.0F, -1.0F/9.0F, 1.0F});
	glUniformMatrix4fv(tms->proj_loc, 1, GL_FALSE, (float *) projection); 
}

static void init_gl(void)
{
	load_pallete();

	load_tms(&g_tms, "res/shaders/tm.geom");
	load_tms(&g_wms, "res/shaders/wm.geom");
	g_tms.tex = load_tile_data("../Poke/Shared/Tiles/TileData00", 0);
	g_wms.tex = load_tile_data("../Poke/Shared/Tiles/TileDataMenu", 2);
}

int main(void) 
{
	set_default_directory();
	init_glfw();
	init_gl();
	set_up_map();

	while (!glfwWindowShouldClose(g_wnd)) {
		glClearColor(0.2F, 0.3F, 0.3F, 1.0F);
		glClear(GL_COLOR_BUFFER_BIT);

		render_tms(&g_tms);
		render_tms(&g_wms);

		glfwSwapBuffers(g_wnd);
		glfwWaitEvents();
	}

	return 0;
}

