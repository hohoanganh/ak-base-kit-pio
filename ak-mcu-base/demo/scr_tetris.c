/**
 * Tetris on a 10 x 20 well of 3-pixel cells, left of the score panel.
 *   B1: left     B2: right     (hold to keep moving)
 *   B3: turn
 *   B1 + B2 held together: drop faster
 * A full row disappears; every 8 rows the pieces fall faster.
 */
#include <string.h>

#include "ui.h"

#define COLS			(10)
#define ROWS			(20)
#define CELL			(3)
#define WELL_X			(2)			/* left edge of the cells */
#define WELL_Y			(2)
#define REPEAT_DELAY	(6)			/* frames a button is held before it repeats */
#define REPEAT_EVERY	(2)

/* 7 pieces x 4 turns, each a 4x4 picture: bit 15 = top left, row by row */
static const uint16_t shapes[7][4] = {
	{ 0x0F00, 0x2222, 0x00F0, 0x4444 },		/* I */
	{ 0x6600, 0x6600, 0x6600, 0x6600 },		/* O */
	{ 0x4E00, 0x4640, 0x0E40, 0x4C40 },		/* T */
	{ 0x6C00, 0x4620, 0x06C0, 0x8C40 },		/* S */
	{ 0xC600, 0x2640, 0x0C60, 0x4C80 },		/* Z */
	{ 0x8E00, 0x6440, 0x0E20, 0x44C0 },		/* J */
	{ 0x2E00, 0x4460, 0x0E80, 0xC440 },		/* L */
};

static uint16_t well[ROWS];				/* bit x set = cell (x, row) is filled */
static uint8_t piece, turn, next_piece;
static int8_t px, py;					/* top left of the 4x4 picture, in cells */
static uint8_t state;					/* 0 ready, 1 playing, 2 over */
static uint16_t score, lines, best;
static uint8_t fall_count, hold_count, idle_frames;
static int8_t goal_x;					/* autoplay: where the piece should land */
static uint8_t goal_turn, goal_valid;
static uint8_t auto_drop;				/* autoplay: the piece is in place, let it fall */

static uint8_t shape_bit(uint8_t p, uint8_t t, int cx, int cy) {
	return (uint8_t)((shapes[p][t] >> (15 - (cy * 4 + cx))) & 1);
}

static uint8_t fits(uint8_t p, uint8_t t, int x, int y) {
	for (int cy = 0; cy < 4; cy++) {
		for (int cx = 0; cx < 4; cx++) {
			if (shape_bit(p, t, cx, cy)) {
				int wx = x + cx, wy = y + cy;

				if (wx < 0 || wx >= COLS || wy >= ROWS) {
					return 0;
				}
				if (wy >= 0 && ((well[wy] >> wx) & 1)) {
					return 0;
				}
			}
		}
	}
	return 1;
}

static void spawn(void) {
	piece = next_piece;
	next_piece = (uint8_t)(ui_rand() % 7);
	turn = 0;
	px = 3;
	py = -1;
	fall_count = 0;
	goal_valid = 0;
	auto_drop = 0;
	if (!fits(piece, turn, px, py)) {
		state = 2;
		idle_frames = 0;
		if (score > best) {
			best = score;
		}
		ui_beep(250, 400);
	}
}

static void tetris_enter(void) {
	memset(well, 0, sizeof(well));
	score = 0;
	lines = 0;
	state = 0;
	hold_count = 0;
	idle_frames = 0;
	next_piece = (uint8_t)(ui_rand() % 7);
	spawn();
	state = 0;
}

static void lock(void) {
	uint8_t cleared = 0;

	for (int cy = 0; cy < 4; cy++) {
		for (int cx = 0; cx < 4; cx++) {
			if (shape_bit(piece, turn, cx, cy) && py + cy >= 0) {
				well[py + cy] |= (uint16_t)(1 << (px + cx));
			}
		}
	}
	for (int y = ROWS - 1; y >= 0; y--) {
		while (well[y] == (1 << COLS) - 1) {
			memmove(&well[1], &well[0], (size_t)y * sizeof(well[0]));
			well[0] = 0;
			cleared++;
		}
	}
	if (cleared) {
		static const uint8_t points[5] = { 0, 1, 3, 5, 8 };

		lines = (uint16_t)(lines + cleared);
		score = (uint16_t)(score + points[cleared] * 10);
		ui_beep(2400, 60);
	}
	else {
		score++;
		ui_beep(700, 10);
	}
	spawn();
}

static void move(int dx) {
	if (fits(piece, turn, px + dx, py)) {
		px = (int8_t)(px + dx);
	}
}

static void rotate(void) {
	uint8_t t = (uint8_t)((turn + 1) & 3);

	/* next to a wall the turned piece may only fit one or two cells further in */
	for (int8_t kick = 0; kick <= 2; kick++) {
		if (fits(piece, t, px + kick, py)) {
			px = (int8_t)(px + kick);
			turn = t;
			return;
		}
		if (kick && fits(piece, t, px - kick, py)) {
			px = (int8_t)(px - kick);
			turn = t;
			return;
		}
	}
}

static void tetris_key(uint8_t btn) {
	if (state == 2) {
		tetris_enter();
		return;
	}
	state = 1;
	if (btn == KIT_BTN_1) {
		move(-1);
	}
	else if (btn == KIT_BTN_2) {
		move(1);
	}
	else {
		rotate();
	}
}

/*----------------------------------------------------------------------------
 * autoplay: try every turn and column, keep the landing place that leaves the
 * lowest pile with the fewest covered holes
 *--------------------------------------------------------------------------*/
static void plan(void) {
	int32_t best_cost = 0x7FFFFFFF;

	for (uint8_t t = 0; t < 4; t++) {
		for (int x = -2; x < COLS; x++) {
			int y = py;
			int32_t cost = 0;
			uint16_t saved[ROWS];

			if (!fits(piece, t, x, y)) {
				continue;
			}
			while (fits(piece, t, x, y + 1)) {
				y++;
			}
			memcpy(saved, well, sizeof(well));
			for (int cy = 0; cy < 4; cy++) {
				for (int cx = 0; cx < 4; cx++) {
					if (shape_bit(piece, t, cx, cy) && y + cy >= 0) {
						well[y + cy] |= (uint16_t)(1 << (x + cx));
					}
				}
			}
			for (int cx = 0; cx < COLS; cx++) {
				uint8_t roof = 0;

				for (int wy = 0; wy < ROWS; wy++) {
					if ((well[wy] >> cx) & 1) {
						if (!roof) {
							roof = 1;
							cost += (ROWS - wy) * (ROWS - wy);		/* high columns cost much more */
						}
					}
					else if (roof) {
						cost += 60;									/* a hole under a block */
					}
				}
			}
			for (int wy = 0; wy < ROWS; wy++) {
				if (well[wy] == (1 << COLS) - 1) {
					cost -= 200;									/* a full row */
				}
			}
			memcpy(well, saved, sizeof(well));
			if (cost < best_cost) {
				best_cost = cost;
				goal_x = (int8_t)x;
				goal_turn = t;
			}
		}
	}
	goal_valid = 1;
}

static void autopilot(void) {
	if (!goal_valid) {
		plan();
	}
	if (turn != goal_turn) {
		uint8_t before = turn;

		rotate();
		if (turn == before) {
			goal_turn = turn;			/* cannot turn here: take it as it is */
		}
	}
	else if (px < goal_x) {
		move(1);
	}
	else if (px > goal_x) {
		move(-1);
	}
	else {
		auto_drop = 1;
	}
}

static void step(void) {
	uint8_t held = (uint8_t)(kit_buttons() & (KIT_BTN_1 | KIT_BTN_2));
	uint8_t level = (uint8_t)(lines / 8);
	uint8_t fall_every = (uint8_t)(level >= 9 ? 2 : 11 - level);

	if (held == (KIT_BTN_1 | KIT_BTN_2) || auto_drop) {
		fall_every = 1;					/* both buttons: drop */
		hold_count = 0;
	}
	else if (held) {
		if (++hold_count > REPEAT_DELAY && (hold_count - REPEAT_DELAY) % REPEAT_EVERY == 0) {
			move(held == KIT_BTN_1 ? -1 : 1);
		}
	}
	else {
		hold_count = 0;
	}

	if (++fall_count >= fall_every) {
		fall_count = 0;
		if (fits(piece, turn, px, py + 1)) {
			py++;
		}
		else {
			lock();
		}
	}
}

static void cell(int x, int y) {
	/* a ring of 3x3 pixels: neighbouring cells stay apart */
	gfx_fill(WELL_X + x * CELL, WELL_Y + y * CELL, CELL, CELL, 1);
	gfx_pixel(WELL_X + x * CELL + 1, WELL_Y + y * CELL + 1, 0);
}

static void tetris_frame(uint32_t now_ms) {
	char buf[12];

	(void)now_ms;
	if (ui_autoplay) {
		if (state != 1) {
			if (++idle_frames >= (state == 0 ? 10 : 40)) {
				tetris_key(KIT_BTN_3);
			}
		}
		else {
			autopilot();
		}
	}
	if (state == 1) {
		step();
	}

	gfx_clear();
	gfx_rect(WELL_X - 2, WELL_Y - 2, COLS * CELL + 4, ROWS * CELL + 4, 1);
	for (int y = 0; y < ROWS; y++) {
		for (int x = 0; x < COLS; x++) {
			if ((well[y] >> x) & 1) {
				cell(x, y);
			}
		}
	}
	if (state != 2) {
		for (int cy = 0; cy < 4; cy++) {
			for (int cx = 0; cx < 4; cx++) {
				if (shape_bit(piece, turn, cx, cy) && py + cy >= 0) {
					gfx_fill(WELL_X + (px + cx) * CELL, WELL_Y + (py + cy) * CELL, CELL, CELL, 1);
				}
			}
		}
	}

	/* panel */
	gfx_text(40, 0, "TETRIS", 1);
	gfx_hline(40, 9, GFX_W - 40, 1);
	gfx_text(40, 13, "score", 1);
	gfx_text(GFX_W - gfx_text_width(gfx_utoa(buf, score, 1), 1), 13, buf, 1);
	gfx_text(40, 23, "rows", 1);
	gfx_text(GFX_W - gfx_text_width(gfx_utoa(buf, lines, 1), 1), 23, buf, 1);
	gfx_text(40, 33, "next", 1);
	for (int cy = 0; cy < 4; cy++) {
		for (int cx = 0; cx < 4; cx++) {
			if (shape_bit(next_piece, 0, cx, cy)) {
				gfx_fill(104 + cx * 4, 31 + cy * 4, 3, 3, 1);
			}
		}
	}
	if (state == 2) {
		gfx_text(40, 46, "GAME OVER", 1);
		gfx_text(40, 56, "best", 1);
		gfx_text(GFX_W - gfx_text_width(gfx_utoa(buf, best, 1), 1), 56, buf, 1);
	}
	else {
		gfx_text(40, 46, "B1< B2> B3turn", 1);
		gfx_text(40, 56, "B1+B2 drop", 1);
	}
}

const ui_screen_t scr_tetris = { "Tetris", tetris_enter, tetris_key, tetris_frame, 0 };
