/**
 * Pong for two kits on one RS485 cable, or one kit against the computer.
 *   B1: up     B2: down     (hold)
 * Your paddle is always the left one.
 *
 * Alone, the kit plays against itself and calls "hello" on the RS485 line.
 * When a second kit answers, the one with the higher number becomes the host:
 * it moves the ball and sends the game 20 times a second; the guest answers
 * each time with the position of its paddle and shows the field mirrored.
 * While this screen is open the RS485 port belongs to the game, not to Modbus.
 *
 * Frames (same speed as Modbus, 9600 8N1):
 *   C5 'H' id(2)                                             crc16
 *   C5 'S' seq ball_x ball_y host_paddle score_h score_g ev  crc16   host -> guest
 *   C5 'P' seq guest_paddle                                  crc16   guest -> host
 * The guest answers right after a state frame, so the two never talk at once.
 */
#include <string.h>

#include "ui.h"
#include "crc.h"

#define TOP				(10)
#define PADDLE_H		(12)
#define PADDLE_X		(2)			/* left edge of the local paddle; the other one is mirrored */
#define PADDLE_STEP		(3)
#define BALL			(2)
#define Y_MIN			(TOP)
#define Y_MAX			(GFX_H - PADDLE_H)
#define LINK_TIMEOUT	(20)		/* frames without the other kit: alone again */

#define SOF				(0xC5)
#define EV_BOUNCE		(0x01)
#define EV_POINT		(0x02)

enum { ROLE_SOLO = 0, ROLE_HOST, ROLE_GUEST };

static uint8_t role;
static uint16_t my_id;
static uint8_t quiet_frames;		/* frames since the other kit was heard */
static uint8_t hello_in;			/* frames until the next hello */
static uint8_t seq;

/* the game as the host sees it: host paddle left, other paddle right */
static int16_t bx, by, vx, vy;		/* ball, 1/16 pixel */
static uint8_t pad_host, pad_other;	/* top of each paddle */
static uint8_t score_host, score_other;
static uint8_t serve_in;			/* frames until the ball starts */
static uint8_t events;

/* guest: what the last state frame said */
static uint8_t g_ball_x, g_ball_y;

static int8_t move_frames, move_dir;

/* receiver */
static uint8_t rx[12];			/* the longest frame (state) is 11 bytes */
static uint8_t rx_len, rx_need;

static void send(uint8_t type, const uint8_t* payload, uint8_t n) {
	uint8_t f[12];
	uint16_t crc;

	f[0] = SOF;
	f[1] = type;
	memcpy(&f[2], payload, n);
	crc = crc16_update(CRC16_INIT, &f[1], (uint32_t)n + 1);
	f[2 + n] = (uint8_t)crc;
	f[3 + n] = (uint8_t)(crc >> 8);
	ui_link_write(f, (uint8_t)(n + 4));
}

static void new_id(void) {
	my_id = (uint16_t)(1 + ui_rand() % 0xFFFE);
}

static void serve(int8_t dir) {
	bx = (GFX_W / 2 - 1) * 16;
	by = (int16_t)((TOP + 8 + ui_rand() % 36) * 16);
	vx = (int16_t)(dir * 26);
	vy = (int16_t)((ui_rand() & 1) ? 12 : -12);
	serve_in = 20;
}

static void new_game(void) {
	score_host = 0;
	score_other = 0;
	pad_host = pad_other = (TOP + GFX_H - PADDLE_H) / 2;
	serve((ui_rand() & 1) ? 1 : -1);
}

static void set_role(uint8_t r) {
	if (r != role) {
		role = r;
		new_game();
		ui_beep(r == ROLE_SOLO ? 600 : 2400, 80);
	}
	quiet_frames = 0;
}

/*----------------------------------------------------------------------------
 * link
 *--------------------------------------------------------------------------*/
static void got_frame(uint8_t type, const uint8_t* p) {
	if (type == 'H') {
		uint16_t id = (uint16_t)(p[0] | (p[1] << 8));

		if (id == my_id) {
			new_id();					/* same number: draw again */
		}
		else if (role == ROLE_SOLO) {
			set_role(id < my_id ? ROLE_HOST : ROLE_GUEST);
		}
	}
	else if (type == 'S') {
		uint8_t reply[2];

		if (role == ROLE_HOST) {		/* two hosts: both start over */
			new_id();
			set_role(ROLE_SOLO);
			return;
		}
		set_role(ROLE_GUEST);
		g_ball_x = p[1];
		g_ball_y = p[2];
		pad_host = p[3];
		score_host = p[4];
		score_other = p[5];
		if (p[6] & EV_POINT) {
			ui_beep(300, 150);
		}
		else if (p[6] & EV_BOUNCE) {
			ui_beep(1100, 8);
		}
		reply[0] = p[0];
		reply[1] = pad_other;
		send('P', reply, 2);
	}
	else if (type == 'P' && role == ROLE_HOST) {
		quiet_frames = 0;
		if (p[1] >= Y_MIN && p[1] <= Y_MAX) {
			pad_other = p[1];
		}
	}
}

void pong_poll(void) {
	int c;

	while ((c = ui_link_getc()) >= 0) {
		if (rx_len == 0) {
			if (c == SOF) {
				rx[rx_len++] = SOF;
			}
			continue;
		}
		rx[rx_len++] = (uint8_t)c;
		if (rx_len == 2) {
			/* payload length by type, plus start, type and two CRC bytes */
			rx_need = (c == 'H') ? 6 : (c == 'S') ? 11 : (c == 'P') ? 6 : 0;
			if (!rx_need) {
				rx_len = 0;
			}
		}
		else if (rx_len == rx_need) {
			uint16_t crc = crc16_update(CRC16_INIT, &rx[1], (uint32_t)rx_need - 3);

			rx_len = 0;
			if (rx[rx_need - 2] == (uint8_t)crc && rx[rx_need - 1] == (uint8_t)(crc >> 8)) {
				got_frame(rx[1], &rx[2]);
			}
		}
	}
}

/*----------------------------------------------------------------------------
 * game
 *--------------------------------------------------------------------------*/
static uint8_t clamp_pad(int y) {
	return (uint8_t)(y < Y_MIN ? Y_MIN : y > Y_MAX ? Y_MAX : y);
}

/* a paddle that plays by itself: follows the ball when it comes its way */
static uint8_t robot(uint8_t pad, uint8_t coming, uint8_t speed) {
	int target = coming ? by / 16 - PADDLE_H / 2 + 1 : (TOP + GFX_H - PADDLE_H) / 2;

	if (target > pad + speed) {
		return clamp_pad(pad + speed);
	}
	if (target < pad - speed) {
		return clamp_pad(pad - speed);
	}
	return pad;
}

static void point(uint8_t host_scored) {
	if (host_scored) {
		score_host++;
	}
	else {
		score_other++;
	}
	if (score_host > 99 || score_other > 99) {
		score_host = score_other = 0;
	}
	events |= EV_POINT;
	ui_beep(300, 150);
	serve(host_scored ? 1 : -1);
}

static void paddle_hit(uint8_t pad, int8_t dir) {
	int off = by / 16 + BALL / 2 - (pad + PADDLE_H / 2);		/* -7 .. 7 */
	int speed = (vx < 0 ? -vx : vx) + 2;

	if (speed > 44) {
		speed = 44;
	}
	vx = (int16_t)(dir * speed);
	vy = (int16_t)(off * 5);
	events |= EV_BOUNCE;
	ui_beep(1100, 8);
}

static void move_ball(void) {
	int x, y;

	if (serve_in) {
		serve_in--;
		return;
	}
	bx = (int16_t)(bx + vx);
	by = (int16_t)(by + vy);
	x = bx / 16;
	y = by / 16;
	if (y < TOP) {
		by = TOP * 16;
		vy = (int16_t)-vy;
		events |= EV_BOUNCE;
	}
	else if (y > GFX_H - BALL) {
		by = (GFX_H - BALL) * 16;
		vy = (int16_t)-vy;
		events |= EV_BOUNCE;
	}
	y = by / 16;

	if (vx < 0 && x <= PADDLE_X + 1) {
		if (y + BALL > pad_host && y < pad_host + PADDLE_H) {
			bx = (PADDLE_X + 2) * 16;
			paddle_hit(pad_host, 1);
		}
		else if (bx < 0) {
			point(0);
		}
	}
	else if (vx > 0 && x + BALL >= GFX_W - PADDLE_X - 2) {
		if (y + BALL > pad_other && y < pad_other + PADDLE_H) {
			bx = (int16_t)((GFX_W - PADDLE_X - 2 - BALL) * 16);
			paddle_hit(pad_other, -1);
		}
		else if (x > GFX_W - BALL) {
			point(1);
		}
	}
}

static void pong_enter(void) {
	ui_link_open();
	new_id();
	role = ROLE_SOLO;
	rx_len = 0;
	hello_in = 5;
	move_frames = 0;
	new_game();
}

static void pong_leave(void) {
	ui_link_close();
}

static void pong_key(uint8_t btn) {
	if (btn != KIT_BTN_3) {
		move_dir = (btn == KIT_BTN_1) ? -1 : 1;
		move_frames = 3;
	}
}

static void pong_frame(uint32_t now_ms) {
	char buf[12];
	uint8_t held = kit_buttons();
	uint8_t* mine = (role == ROLE_GUEST) ? &pad_other : &pad_host;
	int dir = 0;
	int x;

	(void)now_ms;
	pong_poll();

	/* my paddle */
	if (ui_autoplay && role != ROLE_GUEST) {
		*mine = robot(*mine, vx < 0, 2);
	}
	else if (ui_autoplay) {
		int target = g_ball_y - PADDLE_H / 2;

		*mine = clamp_pad(*mine + (target > *mine + 2 ? 2 : target < *mine - 2 ? -2 : 0));
	}
	else {
		if (move_frames) {
			move_frames--;
			dir = move_dir;
		}
		else if (held & KIT_BTN_1) {
			dir = -1;
		}
		else if (held & KIT_BTN_2) {
			dir = 1;
		}
		*mine = clamp_pad(*mine + dir * PADDLE_STEP);
	}

	if (role != ROLE_SOLO && ++quiet_frames > LINK_TIMEOUT) {
		set_role(ROLE_SOLO);
	}

	if (role == ROLE_SOLO) {
		pad_other = robot(pad_other, vx > 0, 2);		/* the kit plays the other side */
		move_ball();
		events = 0;
		if (hello_in-- == 0) {
			uint8_t p[2] = { (uint8_t)my_id, (uint8_t)(my_id >> 8) };

			send('H', p, 2);
			hello_in = (uint8_t)(8 + ui_rand() % 8);	/* uneven, so two kits do not keep colliding */
		}
	}
	else if (role == ROLE_HOST) {
		uint8_t p[7];

		move_ball();
		p[0] = ++seq;
		p[1] = (uint8_t)(bx < 0 ? 0 : bx / 16);
		p[2] = (uint8_t)(by / 16);
		p[3] = pad_host;
		p[4] = score_host;
		p[5] = score_other;
		p[6] = events;
		events = 0;
		send('S', p, 7);
	}

	gfx_clear();
	{
		char* s = buf;
		uint8_t left = (role == ROLE_GUEST) ? score_other : score_host;
		uint8_t right = (role == ROLE_GUEST) ? score_host : score_other;

		gfx_utoa(s, left, 1);
		s += strlen(s);
		*s++ = ' ';
		*s++ = ':';
		*s++ = ' ';
		gfx_utoa(s, right, 1);
	}
	gfx_text((GFX_W - gfx_text_width(buf, 1)) / 2, 1, buf, 1);		/* the title bar inverts it with the rest */
	ui_title("PONG", role == ROLE_SOLO ? "alone" : role == ROLE_HOST ? "host" : "guest");
	for (int y = TOP + 1; y < GFX_H; y += 6) {			/* the net */
		gfx_vline(GFX_W / 2, y, 3, 1);
	}
	/* the local player is always on the left: the guest shows everything mirrored */
	if (role == ROLE_GUEST) {
		gfx_fill(PADDLE_X, pad_other, 2, PADDLE_H, 1);
		gfx_fill(GFX_W - PADDLE_X - 2, pad_host, 2, PADDLE_H, 1);
		x = GFX_W - BALL - g_ball_x;
		gfx_fill(x, g_ball_y, BALL, BALL, 1);
	}
	else {
		gfx_fill(PADDLE_X, pad_host, 2, PADDLE_H, 1);
		gfx_fill(GFX_W - PADDLE_X - 2, pad_other, 2, PADDLE_H, 1);
		gfx_fill(bx / 16, by / 16, BALL, BALL, 1);
	}
}

const ui_screen_t scr_pong = { "Pong RS485", pong_enter, pong_key, pong_frame, pong_leave };
