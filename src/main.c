#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include <windows.h>

#include <cglm/cglm.h>
#include <glad/glad.h>
#include <glfw/glfw3.h>

#include "render.h"
#include "xstd.h"

#define WND_WIDTH 640 
#define WND_HEIGHT 576 

#define SIZEOF_TILE_MAP (TILE_MAP_LEN * TILE_MAP_LEN) 

#define MIN(a, b) ((a) < (b) ? (a) : (b))

#define TEXT_SIZE 256

#define TF_PLACE 1

#define BEG_KEY GLFW_KEY_SPACE 
#define END_KEY (GLFW_KEY_RIGHT_BRACKET + 1)
#define COUNTOF_KEY_TO_CHAR (END_KEY - BEG_KEY)

#define IKEY(key) ((key) - BEG_KEY) 

#define MAX_MAP_PATH 16 

#define MAX_TEXTS 32 
#define MAX_OBJS  32 
#define MAX_DOORS 32 

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

struct sel {
	struct v2b pos;
	struct v2b dpos;
	struct v2b tl;
	struct v2b br;
	int blank;
};

struct text {
	struct v2b pos;
	char str[TEXT_SIZE];
};

struct object {
	struct v2b pos;
	uint8_t dir;
	uint8_t speed;
	uint8_t tile;
	char str[TEXT_SIZE];
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

static GLFWwindow *g_wnd;
static int g_vx;
static int g_vy;
static int g_vw = WND_WIDTH;
static int g_vh = WND_HEIGHT;

static ivec2 g_cam;

static int g_qm_width;
static int g_qm_height;
static map_row g_qm_data[256];
static int g_def_quad;

static int g_text_count;
static struct text g_texts[MAX_TEXTS];

static int g_object_count;
static struct object g_objects[MAX_OBJS];

static uint8_t g_quad_data[128][2][2];
static uint8_t g_qprops[128];

static int g_place;
static int g_qsel_page;
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

static struct sel g_msel = {
	.pos = {2, 3},
	.dpos = {0, 2}, 
	.tl = {2, 3},
	.br = {2, 9},
	.blank = MT_BLANK
};

static char g_path[MAX_MAP_PATH];

static uint8_t g_txt_flags;

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

static void destroy_text(struct text *t)
{
	*t = g_texts[--g_text_count];
}

static struct text *find_text(int x, int y)
{
	struct text *t;
	int n;

	t = g_texts;
	n = g_text_count;
	while (n--) {
		if (t->pos.x == x && t->pos.y == y) {
			return t;
		}
		t++;
	}
	return NULL;
}

static struct text *get_text(int x, int y)
{
	struct text *t;

	t = find_text(x, y);
	if (t) {
		return t;
	}

	if (g_text_count >= MAX_TEXTS) {
		return NULL;
	}

	t = g_texts + g_text_count++;
	t->pos.x = 0;
	t->pos.y = 0;
	*t->str = 0;
	return t;
}

static void place_quad(void)
{	
	int tx, ty;
	int qx, qy;
	uint8_t *q;

	tx = g_qm_sel.pos.x + g_tms.scroll.x / 8; 
	ty = g_qm_sel.pos.y + g_tms.scroll.y / 8; 

	qx = g_cam[0] + g_qm_sel.pos.x / 2;
	qy = g_cam[1] + g_qm_sel.pos.y / 2;

	q = &g_qm_data[qy][qx]; 
	if (g_qprops[*q] & QP_MSG) {
		struct text *t;

		t = find_text(qx, qy);
		if (t) {
			destroy_text(t);
		}
	} else if (g_qprops[*q] & QP_DOOR) {
	}
	*q = g_place; 

	mq_to_t(tx, ty, qx, qy);
}

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

static void place_box(int x0, int y0, int x1, int y1)
{
	tile_row *row;
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

static void clear_wm(void)
{
	memset(g_wms.tm, 0, sizeof(g_wms.tm));
}

static void open_edit(void);

static char *g_base;
static char *g_lines[3];
static struct v2b g_cursor; 

static int is_sep(int ch)
{
	return isspace(ch) || ch == '-'; 
}

static size_t word_len(const char *str)
{
	const char *s;

	s = str;
	if (is_sep(*s)) {
		while (*s && is_sep(*s)) {
			s++;
		}
	} else {
		while (*s && !is_sep(*s)) {
			s++;
		}
	}

	return s - str;
}

static char *next_line(char *line)
{
	char *s;
	size_t n;
		
	s = line;
	n = 17;
	while (*s) {
		char *newline;
		size_t len;

		/*when newline is not needed but still used*/
		if (*s == '\n') {
			s++;
		}

		len = word_len(s);
		if (len > 17) {
			len = 17;
		}

		if (len > n) {
			return s;
		}

		n -= len;
		
		newline = memchr(s, '\n', len + 1);
		if (newline) {
			return newline + 1;
		}
		s += len;
	}
	return s;
}

static size_t word_len_rev(const char *str, const char *base)
{
	const char *s;

	s = str;
	if (is_sep(s[-1])) {
		while (s > base && is_sep(s[-1])) {
			s--;
		}
	} else {
		while (s > base && !is_sep(s[-1])) {
			s--;
		}
	}
	return str - s;
}

static char *prev_line(char *line, char *base)
{
	char *s;
	size_t n;

	s = line;
	n = 17;

	while (s > base) {
		char *newline;
		size_t len;

		if (s[-1] == '\n') {
			s--;
		}

		len = word_len_rev(s, base); 
		if (len > 17) {
			len = 17;
		}

		if (len > n) {
			return s;
		}

		n -= len;
		
		newline = memchr(s - len - 1, '\n', len + 1);
		if (newline) {
			return newline + 1;
		}
		s -= len;
	}

	return s;
}

static void place_lines(void)
{
	int y;
	char **ps;

	y = 14;
	ps = g_lines;
	while (y <= 16) {
		int x;
		const char *s, *end;

		/*place text*/
		x = 1;
		s = ps[0];
		end = ps[1];
		while (s < end && *s != '\n') {
			if (x == g_cursor.x && y == g_cursor.y) {
				g_wms.tm[y][x] = MT_FULL_HORZ_ARROW; 
			} else {
				g_wms.tm[y][x] = ch_to_tile(*s);
				s++;
			}
			x++;
		}

		/*pad text with blank tiles*/
		for (; x < 19; x++) {
			if (x == g_cursor.x && y == g_cursor.y) {
				g_wms.tm[y][x] = MT_FULL_HORZ_ARROW; 
			} else {
				g_wms.tm[y][x] = MT_BLANK;
			}
		}

		/*next line*/
		y += 2;
		ps++;
	}
}


static char *cur_lptr(void)
{
	return g_lines[(g_cursor.y - 14) / 2];
}

static char *next_lptr(void)
{
	return g_lines[(g_cursor.y - 14) / 2 + 1];
}

static size_t dif_line(void)
{
	return next_lptr() - cur_lptr();
}

static int is_single_line(void)
{
	return g_lines[1] == g_lines[2];
}

static void horz_cursor_lim(void)
{
	size_t dif;
	dif = dif_line() + 1;
	g_cursor.x = MIN(g_cursor.x, dif);
}

static void forw_line(void)
{
	char *line;

	line = next_line(g_lines[2]); 
	if (line != g_lines[2]) {
		g_lines[0] = g_lines[1];
		g_lines[1] = g_lines[2]; 
		g_lines[2] = line;
	} else if (!is_single_line()) {
		g_cursor.y = 16;
	}

	horz_cursor_lim();
	g_txt_flags |= TF_PLACE;
}

static void back_line(void)
{
	char *line;

	line = prev_line(g_lines[0], g_base); 
	if (line != g_lines[0]) {
		g_lines[2] = g_lines[1];
		g_lines[1] = g_lines[0]; 
		g_lines[0] = line;
	} else if (!is_single_line()) {
		g_cursor.y = 14;
	}

	horz_cursor_lim();
	g_txt_flags |= TF_PLACE;
}

static struct v2b cur_pos(void)
{
	struct v2b v;
	v.x = g_cursor.x - 1; 
	v.y = (g_cursor.y - 14) / 2;
	return v;
}

static void fix_cursor(const char *s)
{
	if (!is_single_line() && s >= g_lines[1]) {
		g_cursor.x = s - g_lines[1] + 1;
		g_cursor.y = 16;
	} else if (s >= g_lines[0]) {
		g_cursor.x = s - g_lines[0] + 1;
		g_cursor.y = 14;
	}
}

static int next_char(void)
{
	struct v2b v; 
	const char *s;

	v = cur_pos();

	s = &g_lines[v.y][v.x];
	if (!*s) {
		return *s;
	} 

	s++;
	if (s >= g_lines[v.y + 1]) {
		forw_line();
	}
	fix_cursor(s);
	g_txt_flags |= TF_PLACE;
	return *s;
}

static const char *prev_char(void)
{
	struct v2b v;
	const char *s;

	v = cur_pos();

	s = &g_lines[v.y][v.x];
	if (s == g_base) {
		return s;
	} 

	s--;
	if (s < g_lines[0]) {
		back_line();
	}
	fix_cursor(s);
	g_txt_flags |= TF_PLACE;
	return s;
}

static void next_word(void)
{
	int ch;
	do {
		ch = next_char();
	} while (ch && !isspace(ch));
	do {
		ch = next_char();
	} while (ch && isspace(ch));
}

static void prev_word(void)
{
	const char *s;
	do {
		s = prev_char();
	} while (s != g_base && isspace(*s));
	do {
		s = prev_char();
	} while (s != g_base && !isspace(*s));
	if (s != g_base) {
		next_char();
	}
}

static void insert_ch(char *s, int ch) 
{
	do {
		int tmp;
		tmp = ch;
		ch = *s;
		*s = tmp;
	} while (*s++);
}

static void remove_ch(char *s)
{
	while ((s[0] = s[1])) {
		s++;
	}
}

static void insert_key(int ch)
{
	struct v2b v;
	char *b;
	size_t size;

	size = strlen(g_base) + 1; 
	if (size >= TEXT_SIZE) {
		return;	
	}

	v = cur_pos();
	b = &g_lines[v.y][v.x];

	insert_ch(b, ch);

	g_lines[1] = next_line(g_lines[0]);
	g_lines[2] = next_line(g_lines[1]);
	next_char();
	g_txt_flags |= TF_PLACE;
}

static void remove_key(void)
{
	struct v2b v;
	char *b;

	v = cur_pos();
	b = &g_lines[v.y][v.x];

	if (b == g_base) {
		return;
	}

	remove_ch(b - 1);

	g_lines[1] = next_line(g_lines[0]);
	g_lines[2] = next_line(g_lines[1]);
	prev_char();

	g_txt_flags |= TF_PLACE;
}

static void quad_key_cb(GLFWwindow *wnd, int key, 
		int scancode, int action, int mods)
{
	switch (key) {
	case GLFW_KEY_ESCAPE:
		switch (action) {
		case GLFW_PRESS:
			clear_wm();
			open_edit();
			break;
		}
		break;
	case GLFW_KEY_DOWN:
		switch (action) {
		case GLFW_PRESS:
		case GLFW_REPEAT:
			forw_line();
			break;
		}
		break;
	case GLFW_KEY_UP:
		switch (action) {
		case GLFW_PRESS:
		case GLFW_REPEAT:
			back_line();
			break;
		}
		break;
	case GLFW_KEY_LEFT:
		switch (action) {
		case GLFW_PRESS:
		case GLFW_REPEAT:
			if (mods & GLFW_MOD_CONTROL) {
				prev_word();
			} else {
				prev_char();
			}
			break;
		}
		break;
	case GLFW_KEY_RIGHT:
		switch (action) {
		case GLFW_PRESS:
		case GLFW_REPEAT:
			if (mods & GLFW_MOD_CONTROL) {
				next_word();
			} else {
				next_char();
			}
			break;
		}
		break;
	case GLFW_KEY_BACKSPACE:
		switch (action) {
		case GLFW_PRESS:
		case GLFW_REPEAT:
			remove_key();
			break;
		}
		break;
	}
	if (g_txt_flags & TF_PLACE) {
		place_lines();
		g_txt_flags = 0;
	}
	g_txt_flags = 0;
}

static void quad_char_forw_cb(GLFWwindow *wnd, unsigned cp)
{
	if (cp == '\xE9') {
		cp = 0;
	}
	if (cp == '&') {
		cp = '\xE9';
	}
	if (ch_to_tile(cp)) {
		insert_key(cp);
	}
}

static void quad_char_cb(GLFWwindow *wnd, unsigned cp)
{
	glfwSetCharCallback(g_wnd, quad_char_forw_cb);
}

static void set_state(GLFWkeyfun key, GLFWcharfun ch) 
{
	glfwSetKeyCallback(g_wnd, key);
	glfwSetCharCallback(g_wnd, ch);
}

static void open_quad(void)
{
	int qx, qy;

	qx = g_cam[0] + g_qm_sel.pos.x / 2;
	qy = g_cam[1] + g_qm_sel.pos.y / 2;

	if (g_qprops[g_qm_data[qy][qx]] == QP_MSG) {
		struct text *t;

		t = get_text(qx, qy);
		if (!t) {
			fprintf(stderr, "Too many texts!\n");
			return;
		}
		place_box(0, 12, 20, 19);

		g_base = t->str;
		g_lines[0] = g_base;
		g_lines[1] = next_line(g_lines[0]); 
		g_lines[2] = next_line(g_lines[1]);
		g_cursor.x = 1;
		g_cursor.y = 14;

		place_lines();
		set_state(quad_key_cb, quad_char_cb);
	}	
}

static int g_path_i;

static void open_msel(void);

static void path_key_cb(GLFWwindow *wnd, int key, 
		int scancode, int action, int mods)
{
	switch (key) {
	case GLFW_KEY_BACKSPACE:
		switch (action) {
		case GLFW_PRESS:
		case GLFW_REPEAT:
			if (g_path_i > 0) {
				char *s;
				s = g_path + --g_path_i;
				remove_ch(s);
				place_text(3, 15, g_path);
				g_wms.tm[15][3 + g_path_i] = MT_BLANK;
			}
			break;
		}
		break;
	case GLFW_KEY_ESCAPE:
		open_msel();
		break;
	}
}
static void path_char_forw_cb(GLFWwindow *wnd, unsigned cp)
{
	if (g_path_i + 1 < _countof(g_path) && cp < 0x80 && isalnum(cp)) {
		char *b;
		b = g_path + g_path_i++;
		insert_ch(b, cp);
		place_text(3, 15, g_path);
	}
}

static void path_char_cb(GLFWwindow *wnd, unsigned cp)
{
	glfwSetCharCallback(g_wnd, path_char_forw_cb);
}

static void open_path_sel(void)
{
	place_box(1, 14, 19, 18);
	place_text(3, 15, g_path);
	g_path_i = strlen(g_path);
	set_state(path_key_cb, path_char_cb);
}

static void update_place(void)
{
	ivec2 off;

	off[0] = (g_qsel.pos.x - 2) / 3;
	off[1] = (g_qsel.pos.y - 3) / 3; 
	g_place = g_qsel_page * 20 + off[0] + off[1] * 5; 
}

static void close_msel(void)
{	
	update_place();
	clear_wm();
	open_edit();
}

static void open_qsel(void);

static void msel_key_cb(GLFWwindow *wnd, int key, 
		int scancode, int action, int mods)
{
	if (action != GLFW_PRESS || move_sel_kb(key, &g_msel)) {
		return;
	}

	switch (key) {
	case GLFW_KEY_X:
		switch (g_msel.pos.y) {
		case 3: /*Path*/
			open_path_sel();
			break;
		case 5: /*Open*/
				
			break;
		case 7: /*Save*/
			break;
		case 9: /*Quad*/
			open_qsel();
			break;
		}
		break;
	case GLFW_KEY_Z:
		close_msel();
		break;
	}
}

static void open_msel(void)
{
	place_box(1, 1, 19, 18);
	place_text(3, 3, "Path");
	place_text(3, 5, "Open");
	place_text(3, 7, "Save");
	place_text(3, 9, "Quad");
	place_sel(&g_msel);
	set_state(msel_key_cb, NULL);
}

static void bound_qm_sel(void)
{
	g_qm_sel.br.x = 2 * MIN(9, g_qm_width - g_cam[0] - 1);
	g_qm_sel.br.y = 2 * MIN(8, g_qm_height - g_cam[1] - 1);
}

static void edit_key_cb(GLFWwindow *wnd, int key, 
		int scancode, int action, int mods)
{
	switch (key) {
	case GLFW_KEY_X:
		switch (action) {
		case GLFW_PRESS:
		case GLFW_REPEAT:
			place_quad();
			break;
		}
		break;
	case GLFW_KEY_Z:
		switch (action) {
		case GLFW_PRESS:
			open_quad();
			break;
		}
		break;
	case GLFW_KEY_ENTER:
		switch (action) {
		case GLFW_PRESS:
			remove_sel(&g_qm_sel);
			open_msel();
			break;
		}
		break;
	case GLFW_KEY_RIGHT:
		switch (action) {
		case GLFW_PRESS:
		case GLFW_REPEAT:
			move_cam_right();
			break;
		}
		break;
	case GLFW_KEY_LEFT:
		switch (action) {
		case GLFW_PRESS:
		case GLFW_REPEAT:
			move_cam_left();
			break;
		}
		break;
	case GLFW_KEY_DOWN:
		switch (action) {
		case GLFW_PRESS:
		case GLFW_REPEAT:
			move_cam_down();
			break;
		}
		break;
	case GLFW_KEY_UP:
		switch (action) {
		case GLFW_PRESS:
		case GLFW_REPEAT:
			move_cam_up();
			break;
		}
		break;
	default:
		switch (action) {
		case GLFW_PRESS:
		case GLFW_REPEAT:
			bound_qm_sel();
			move_sel_kb(key, &g_qm_sel);
			break;
		}
	}
}

static void open_edit(void)
{
	g_wms.tm[g_qm_sel.pos.y][g_qm_sel.pos.x] = MT_FULL_HORZ_ARROW;
	set_state(edit_key_cb, NULL);
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

	prop = g_qprops[g_place];
	if (prop < _countof(g_prop_strs)) {
		str = g_prop_strs[prop];	
	} else {
		str = "TBD  ";
	}
	place_textf(6, 3, "%s\n%3d", str, prop);
}

static void mod_prop(int off)
{
	int old;

	old = g_qprops[g_place];
	g_qprops[g_place] += off;

	if (old == QP_MSG) {
		struct text *t; 
		int n;
	
		t = g_texts;
		n = g_text_count;
		while (n--) {
			int q;

			q = g_qm_data[t->pos.y][t->pos.x];		
			if (q == g_place) {
				destroy_text(t);
			}
			t++;
		}
	}
}

static void change_prop(int off)
{
	mod_prop(off);
	place_prop();
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
			set_state(qtsel_key_cb, NULL); 
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
	set_state(tsel_key_cb, NULL); 
}

static void qtsel_key_cb(GLFWwindow *wnd, int key, 
		int scancode, int action, int mode)
{
	if (action == GLFW_PRESS && !move_sel_kb(key, &g_qtsel)) { 
		switch (key) {
		case GLFW_KEY_X:
			open_tsel();
			break;
		case GLFW_KEY_Z:
			open_qsel();
			break;
		case GLFW_KEY_UP:
			change_prop(-1);
			break;
		case GLFW_KEY_DOWN:
			change_prop(1);
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
	set_state(qtsel_key_cb, NULL); 
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

static void close_qsel(void)
{
	tm_to_qm_screen();
	open_msel();
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
	set_state(qsel_key_cb, NULL);
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

	glfwSetInputMode(g_wnd, GLFW_LOCK_KEY_MODS, GLFW_TRUE);
	glfwSetFramebufferSizeCallback(g_wnd, resize_cb);
	open_edit();

	if (!gladLoadGLLoader((GLADloadproc) glfwGetProcAddress)) {
		fprintf(stderr, "glad error\n");
		exit(1);
	}
}

static void fread_all_obj(const char *path, void *buf, size_t size)
{
	FILE *f;

	f = xfopen(path, "rb");
	xfread_obj(f, buf, size); 
	fclose(f);
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

static void read_pstr(FILE *f, char *s, int n)
{
	int len;

	len = xfgetc(f);
	if (n <= len) {
		fprintf(stderr, "Too long of pstr!\n");
		exit(1);
	}
	xfread_obj(f, s, len);
	s[len] = '\0';
}

static void read_texts(FILE *f)
{
	struct text *t;
	int n;

	g_text_count = 0;

	t = g_texts; 
	n = xfgetc(f);
	
	if (n > MAX_TEXTS) {
		fprintf(stderr, "Too many texts!");
		exit(1);
	}

	while (n--) {
		int x, y, q;

		x = xfgetc(f);
		y = xfgetc(f);

		q = g_qm_data[y][x];
		if (g_qprops[q] != QP_MSG) {
			continue;
		}

		t->pos.x = x;
		t->pos.y = y;
		read_pstr(f, t->str, TEXT_SIZE);
		g_text_count++;
		t++;
	}
}

static void read_objects(FILE *f)
{
	struct object *o;
	int n;

	g_object_count = 0;

	o = g_objects;
	n = g_object_count;

	if (n > MAX_OBJS) {
		fprintf(stderr, "Too many objects!");
		exit(1);
	}

	while (n--) {
		o->pos.x = xfgetc(f);
		o->pos.y = xfgetc(f);
		o->dir = xfgetc(f);
		o->speed = xfgetc(f);
		o->tile = xfgetc(f);
		read_pstr(f, o->str, TEXT_SIZE);
		o++;
	}
}

static char *strccpy(char *dst, const char *src)
{
	while ((*dst++ = *src++));
	return dst - 1;
}

static void read_map(const char *path)
{
	FILE *f;
	char full[MAX_PATH];
	char *s;

	s = strccpy(full, "../Poke/Shared/Maps/");
	strcpy(s, path);

	f = fopen(full, "rb");
	if (!f) {
		fprintf(stderr, "map: cannot find map\n");
		return;
	}

	strcpy(g_path, path);

	g_qm_width = xfgetc(f) + 1;
	g_qm_height = xfgetc(f) + 1;
	
	decomp_map_quads(f);
	g_def_quad = xfgetc(f);
	read_texts(f);
	read_objects(f);

	fclose(f);
}

static void set_up_map(void)
{
	static ivec2 origin = {0, 0};
	static ivec4 region = {0, 0, 10, 9};

	fread_all_obj("../Poke/Shared/Tiles/QuadData00", 
			g_quad_data, sizeof(g_quad_data));
	fread_all_obj("../Poke/Shared/Tiles/QuadProps00",
			g_qprops, sizeof(g_qprops));
	
	g_qm_width = 1;
	g_qm_height = 1;
	read_map("PalletTown");
	qm_to_tm(origin, region);
}

int main(void) 
{
	set_default_directory();
	init_glfw();
	init_gl();
	set_up_map();

	while (!glfwWindowShouldClose(g_wnd)) {
		render();
		glfwSwapBuffers(g_wnd);
		glfwWaitEvents();
	}

	return 0;
}

