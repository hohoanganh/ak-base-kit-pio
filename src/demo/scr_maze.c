/**
 * Maze: a first-person walk through a maze, drawn by ray casting (the trick
 * of Wolfenstein 3D), integer math only. Find the striped door.
 *   B1: turn left      B2: turn right     (tap or hold)
 *   B3: walk / stop
 *
 * For each of the 64 screen columns (2 pixels wide) one ray runs through the
 * map grid until it meets a wall; the distance gives the height of the wall
 * slice. Walls facing east/west are white, north/south ones are dithered,
 * far ones are dimmer: that is all the shading a 1-bit display needs.
 *
 * Positions are in 1/256 of a map cell, angles in 1/256 of a turn.
 */
#include "ui.h"

#define MAP_W			(16)
#define MAP_H			(16)
#define VIEW_H			(48)		/* rows 0..47: six display pages */
#define VIEW_CY			(VIEW_H / 2)
#define RAYS			(GFX_W / 2)
#define WALL_SCALE		(44)		/* wall height in pixels at a distance of one cell */
#define WALK_STEP		(20)		/* 1/256 cell per frame */
#define TURN_STEP		(4)			/* 1/256 turn per frame */
#define BODY			(90)		/* keep this far from walls */
#define FAR				(4 * 256)	/* beyond this, walls are drawn dimmer */
#define NEVER			(1L << 28)	/* distance to a grid line the ray runs parallel to */

static const char map[MAP_H][MAP_W + 1] = {
	"################",
	"#......#.......#",
	"#.####.#.#####.#",
	"#.#....#.....#.#",
	"#.#.####.###.#.#",
	"#.#......#...#.#",
	"#.######.#.###.#",
	"#......#.#.#...#",
	"######.#.#.#.###",
	"#......#...#...#",
	"#.##########.#.#",
	"#.#........#.#.#",
	"#.#.######.#.#.#",
	"#.#......#...#.#",
	"#...####.#####.E",
	"################",
};

#define START_X			(1 * 256 + 128)
#define START_Y			(1 * 256 + 128)

typedef struct {
	int32_t dist;			/* distance to the wall, straight ahead of the viewer */
	uint8_t side;			/* 0: wall facing east/west, 1: north/south */
	uint8_t cell_x, cell_y;
	char kind;
} hit_t;

static int32_t px, py;
static uint8_t angle;
static uint8_t walking;
static int8_t turn_dir;				/* turn asked by a key press, for turn_frames frames */
static uint8_t turn_frames;
static uint8_t state;				/* 0 walking around, 1 found the door */
static uint16_t frames_in_state;
static uint32_t start_ms, found_s;
static int8_t auto_turn;			/* autoplay: turning until the way ahead is free */

static char cell(int x, int y) {
	if (x < 0 || y < 0 || x >= MAP_W || y >= MAP_H) {
		return '#';
	}
	return map[y][x];
}

/* One ray from the viewer in direction (rdx, rdy), any length. */
static void cast(int32_t rdx, int32_t rdy, hit_t* h) {
	int mx = (int)(px >> 8), my = (int)(py >> 8);
	int sx = rdx < 0 ? -1 : 1, sy = rdy < 0 ? -1 : 1;
	/* ray length for one cell in x / in y; a ray along an axis never crosses the other grid lines */
	int32_t ddx = rdx ? (127 * 256) / (rdx < 0 ? -rdx : rdx) : 0;
	int32_t ddy = rdy ? (127 * 256) / (rdy < 0 ? -rdy : rdy) : 0;
	int32_t fx = px & 255, fy = py & 255;
	int32_t sdx = rdx ? ((rdx < 0 ? fx : 256 - fx) * ddx) >> 8 : NEVER;
	int32_t sdy = rdy ? ((rdy < 0 ? fy : 256 - fy) * ddy) >> 8 : NEVER;

	h->side = 0;
	for (uint8_t i = 0; i < MAP_W + MAP_H; i++) {
		if (sdx < sdy) {
			sdx += ddx;
			mx += sx;
			h->side = 0;
		}
		else {
			sdy += ddy;
			my += sy;
			h->side = 1;
		}
		if (cell(mx, my) != '.') {
			break;
		}
	}
	h->dist = h->side ? sdy - ddy : sdx - ddx;
	if (h->dist < 16) {
		h->dist = 16;
	}
	h->cell_x = (uint8_t)mx;
	h->cell_y = (uint8_t)my;
	h->kind = cell(mx, my);
}

/* Distance to the wall at an angle relative to where the viewer looks. */
static int32_t look(int8_t rel) {
	uint8_t a = (uint8_t)(angle + rel);
	hit_t h;

	cast(ui_cos(a), ui_sin(a), &h);
	return h.dist;
}

static void maze_enter(void) {
	px = START_X;
	py = START_Y;
	angle = 0;
	walking = 0;
	turn_frames = 0;
	auto_turn = 0;
	state = 0;
	frames_in_state = 0;
	start_ms = 0;
}

static void maze_key(uint8_t btn) {
	if (state == 1) {
		maze_enter();
		return;
	}
	if (btn == KIT_BTN_3) {
		walking = !walking;
		ui_beep(1500, 12);
	}
	else {
		/* a tap turns 1/16 of a circle; holding the button keeps turning (see step) */
		turn_dir = (btn == KIT_BTN_1) ? -1 : 1;
		turn_frames = 4;
	}
}

static void autopilot(void) {
	int32_t ahead = look(0), left = look(-40), right = look(40);

	walking = 1;
	if (auto_turn) {
		angle = (uint8_t)(angle + auto_turn * TURN_STEP);
		if (ahead > 450) {
			auto_turn = 0;
		}
		return;
	}
	if (ahead < 300) {
		auto_turn = (look(-64) > look(64)) ? -1 : 1;	/* wall ahead: turn to the open side */
	}
	else if ((left > 600 || right > 600) && ui_rand() % 32 == 0) {
		auto_turn = (left > right) ? -1 : 1;			/* a side passage: sometimes take it */
	}
	else if (left < 500 && right < 500) {
		/* in a corridor: keep to the middle */
		if (left > right + 60) {
			angle = (uint8_t)(angle - 2);
		}
		else if (right > left + 60) {
			angle = (uint8_t)(angle + 2);
		}
	}
}

static void step(void) {
	uint8_t held = kit_buttons();

	if (turn_frames) {
		turn_frames--;
		angle = (uint8_t)(angle + turn_dir * TURN_STEP);
	}
	else if (held & KIT_BTN_1) {
		angle = (uint8_t)(angle - TURN_STEP);
	}
	else if (held & KIT_BTN_2) {
		angle = (uint8_t)(angle + TURN_STEP);
	}

	if (walking) {
		int32_t dx = ui_cos(angle) * WALK_STEP / 127, dy = ui_sin(angle) * WALK_STEP / 127;
		int nx = (int)((px + dx + (dx < 0 ? -BODY : BODY)) >> 8);
		int ny = (int)((py + dy + (dy < 0 ? -BODY : BODY)) >> 8);

		if (cell(nx, (int)(py >> 8)) == 'E' || cell((int)(px >> 8), ny) == 'E') {
			state = 1;
			frames_in_state = 0;
			walking = 0;
			ui_beep(2600, 300);
			return;
		}
		/* x and y on their own: the viewer slides along a wall instead of sticking to it */
		if (cell(nx, (int)(py >> 8)) == '.') {
			px += dx;
		}
		if (cell((int)(px >> 8), ny) == '.') {
			py += dy;
		}
	}
}

static void draw_view(void) {
	int32_t dx = ui_cos(angle), dy = ui_sin(angle);
	int32_t plx = -dy * 2 / 3, ply = dx * 2 / 3;		/* screen plane: 67 degrees field of view */
	hit_t prev = { 0, 0, 255, 255, 0 };
	uint8_t* fb = gfx_fb();

	for (int c = 0; c < RAYS; c++) {
		int cam = 2 * c - (RAYS - 1);					/* -63 .. 63 */
		hit_t h;
		int height, y0, y1, x = c * 2;
		uint8_t far, new_face;

		cast(dx + plx * cam / RAYS, dy + ply * cam / RAYS, &h);
		height = (int)(WALL_SCALE * 256 / h.dist);
		if (height > VIEW_H) {
			height = VIEW_H;
		}
		y0 = VIEW_CY - height / 2;
		y1 = y0 + height - 1;
		far = h.dist > FAR;
		new_face = (h.side != prev.side || h.cell_x != prev.cell_x || h.cell_y != prev.cell_y) && c > 0;
		prev = h;

		/* The slice goes straight into the frame buffer, a byte (8 rows of one
		 * display column) at a time: a fill pattern cut to the rows y0..y1.
		 * Thousands of gfx_pixel() calls per frame would cost too much. */
		for (int xx = x; xx < x + 2; xx++) {
			uint8_t pattern;

			if (new_face && xx == x) {
				pattern = 0x00;							/* dark seam where one wall face ends */
			}
			else if (h.side == 0) {
				pattern = far ? ((xx & 1) ? 0x55 : 0xAA) : 0xFF;
			}
			else {
				pattern = far ? ((xx & 1) ? 0x00 : 0x55) : ((xx & 1) ? 0x55 : 0xAA);
			}
			for (int page = y0 >> 3; page <= y1 >> 3; page++) {
				int top = page * 8;
				uint8_t rows = 0xFF, v;

				if (y0 > top) {
					rows &= (uint8_t)(0xFF << (y0 - top));
				}
				if (y1 < top + 7) {
					rows &= (uint8_t)(0xFF >> (top + 7 - y1));
				}
				v = pattern & rows;
				if (h.kind == 'E' && pattern) {			/* the door: stripes */
					v = 0;
					for (int y = top; y < top + 8; y++) {
						if (y >= y0 && y <= y1 && (((y - y0) * 8 / height) & 1)) {
							v |= (uint8_t)(1 << (y - top));
						}
					}
				}
				/* top and bottom edge of every wall */
				if (y0 >= top && y0 <= top + 7) {
					v |= (uint8_t)(1 << (y0 - top));
				}
				if (y1 >= top && y1 <= top + 7) {
					v |= (uint8_t)(1 << (y1 - top));
				}
				fb[page * GFX_W + xx] = v;
			}
		}
	}
}

static void maze_frame(uint32_t now_ms) {
	char buf[12];

	if (start_ms == 0) {
		start_ms = now_ms;
	}
	frames_in_state++;
	if (state == 0) {
		if (ui_autoplay) {
			autopilot();
		}
		step();
	}
	else if (ui_autoplay && frames_in_state > 60) {
		maze_enter();
		start_ms = now_ms;
	}
	if (state == 0) {
		found_s = (now_ms - start_ms) / 1000;
	}

	gfx_clear();
	draw_view();
	if (state == 1) {
		gfx_fill(18, 12, 92, 24, 0);
		gfx_rect(18, 12, 92, 24, 1);
		gfx_text_center(15, "EXIT FOUND", 1);
		gfx_text(40, 25, gfx_utoa(buf, found_s, 1), 1);
		gfx_text(40 + gfx_text_width(buf, 1) + 4, 25, "seconds", 1);
	}
	ui_footer("LEFT", "RIGHT", walking ? "STOP" : "WALK");
}

const ui_screen_t scr_maze = { "Maze 3D", maze_enter, maze_key, maze_frame, 0 };
