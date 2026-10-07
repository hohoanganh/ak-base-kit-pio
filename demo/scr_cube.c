/**
 * 3D: a rotating solid drawn with integer math only (no FPU on this chip).
 *   B1: next model (cube, octahedron, pyramid)
 *   B2: spin speed (normal, fast, stopped)
 *   B3: wireframe / shaded
 *
 * Shaded mode: back faces are dropped (the models are convex, so what is left
 * never overlaps), each visible face gets a grey level from the angle to the
 * light and is filled with an ordered dither, since the display is 1 bit.
 */
#include "ui.h"

#define CX			(64)
#define CY			(37)
#define DIST		(170)		/* camera distance, model units */
#define FOCAL		(62)

typedef struct {
	int8_t x, y, z;
} v3_t;

typedef struct {
	uint8_t n;				/* 3 or 4 corners */
	uint8_t v[4];
} face_t;

typedef struct {
	const char* name;
	const v3_t* verts;
	uint8_t nv;
	const face_t* faces;
	uint8_t nf;
} model_t;

static const v3_t cube_v[] = {
	{-32,-32,-32}, { 32,-32,-32}, { 32, 32,-32}, {-32, 32,-32},
	{-32,-32, 32}, { 32,-32, 32}, { 32, 32, 32}, {-32, 32, 32},
};
static const face_t cube_f[] = {
	{4, {0, 1, 2, 3}}, {4, {4, 5, 6, 7}}, {4, {0, 1, 5, 4}},
	{4, {3, 2, 6, 7}}, {4, {0, 3, 7, 4}}, {4, {1, 2, 6, 5}},
};

static const v3_t octa_v[] = {
	{ 52, 0, 0}, {-52, 0, 0}, {0,  52, 0}, {0, -52, 0}, {0, 0,  52}, {0, 0, -52},
};
static const face_t octa_f[] = {
	{3, {0, 2, 4}}, {3, {2, 1, 4}}, {3, {1, 3, 4}}, {3, {3, 0, 4}},
	{3, {2, 0, 5}}, {3, {1, 2, 5}}, {3, {3, 1, 5}}, {3, {0, 3, 5}},
};

static const v3_t pyra_v[] = {
	{-34, 26, -34}, {34, 26, -34}, {34, 26, 34}, {-34, 26, 34}, {0, -40, 0},
};
static const face_t pyra_f[] = {
	{4, {0, 1, 2, 3}}, {3, {0, 1, 4}}, {3, {1, 2, 4}}, {3, {2, 3, 4}}, {3, {3, 0, 4}},
};

static const model_t models[] = {
	{ "cube",    cube_v, 8, cube_f, 6 },
	{ "octa",    octa_v, 6, octa_f, 8 },
	{ "pyramid", pyra_v, 5, pyra_f, 5 },
};

#define MODEL_NUM	((uint8_t)(sizeof(models) / sizeof(models[0])))
#define MAX_VERTS	(8)


static uint32_t isqrt(uint32_t v) {
	uint32_t r = 0, bit = 1UL << 30;

	while (bit > v) {
		bit >>= 2;
	}
	while (bit) {
		if (v >= r + bit) {
			v -= r + bit;
			r = (r >> 1) + bit;
		}
		else {
			r >>= 1;
		}
		bit >>= 2;
	}
	return r;
}

static uint8_t model_idx, speed, solid = 1;
static uint8_t ang_a, ang_b;

static void cube_enter(void) {
}

static void cube_key(uint8_t btn) {
	if (btn == KIT_BTN_1) {
		model_idx = (uint8_t)((model_idx + 1) % MODEL_NUM);
	}
	else if (btn == KIT_BTN_2) {
		speed = (uint8_t)((speed + 1) % 3);
	}
	else {
		solid = !solid;
	}
	ui_beep(1800, 15);
}

static void cube_frame(uint32_t now_ms) {
	const model_t* m = &models[model_idx];
	int rx[MAX_VERTS], ry[MAX_VERTS], rz[MAX_VERTS];	/* rotated */
	int sx[MAX_VERTS], sy[MAX_VERTS];					/* on screen */
	int sa, ca, sb, cb;

	(void)now_ms;
	if (speed != 2) {
		ang_a = (uint8_t)(ang_a + (speed ? 6 : 3));
		ang_b = (uint8_t)(ang_b + (speed ? 4 : 2));
	}
	sa = ui_sin(ang_a); ca = ui_cos(ang_a);
	sb = ui_sin(ang_b); cb = ui_cos(ang_b);

	for (uint8_t i = 0; i < m->nv; i++) {
		int x = m->verts[i].x, y = m->verts[i].y, z = m->verts[i].z;
		int x1 = (x * cb + z * sb) / 127;				/* around the y axis */
		int z1 = (z * cb - x * sb) / 127;
		int y1 = (y * ca - z1 * sa) / 127;				/* around the x axis */
		int z2 = (y * sa + z1 * ca) / 127;

		rx[i] = x1; ry[i] = y1; rz[i] = z2;
		sx[i] = CX + x1 * FOCAL / (z2 + DIST);
		sy[i] = CY + y1 * FOCAL / (z2 + DIST);
	}

	gfx_clear();
	for (uint8_t f = 0; f < m->nf; f++) {
		const face_t* fc = &m->faces[f];
		uint8_t a = fc->v[0], b = fc->v[1], c = fc->v[2];

		if (solid) {
			/* normal of the face, made to point away from the centre */
			int ux = rx[b] - rx[a], uy = ry[b] - ry[a], uz = rz[b] - rz[a];
			int vx = rx[c] - rx[a], vy = ry[c] - ry[a], vz = rz[c] - rz[a];
			int nx = uy * vz - uz * vy, ny = uz * vx - ux * vz, nz = ux * vy - uy * vx;
			int len, lam, level;

			if (nx * (rx[a] + rx[b] + rx[c]) + ny * (ry[a] + ry[b] + ry[c]) + nz * (rz[a] + rz[b] + rz[c]) < 0) {
				nx = -nx; ny = -ny; nz = -nz;
			}
			/* facing away from the camera at (0, 0, -DIST): not drawn */
			if (nx * rx[a] + ny * ry[a] + nz * (rz[a] + DIST) >= 0) {
				continue;
			}
			/* light from the upper left, a little in front: neighbouring faces
			 * get clearly different greys; faces in shadow stay dim, never black */
			len = (int)isqrt((uint32_t)(nx / 4 * (nx / 4) + ny / 4 * (ny / 4) + nz / 4 * (nz / 4)));
			lam = len ? -((nx / 4) * 80 + (ny / 4) * 70 + (nz / 4) * 70) / len : 0;	/* -127..127 */
			level = 2 + (lam > 0 ? lam * 15 / 127 : 0);
			if (level > 16) {
				level = 16;
			}

			gfx_tri(sx[a], sy[a], sx[b], sy[b], sx[c], sy[c], (uint8_t)level);
			if (fc->n == 4) {
				uint8_t d = fc->v[3];
				gfx_tri(sx[a], sy[a], sx[c], sy[c], sx[d], sy[d], (uint8_t)level);
			}
		}
		/* edges: white lines for the wireframe, dark seams between the shaded faces */
		for (uint8_t i = 0; i < fc->n; i++) {
			uint8_t p = fc->v[i], q = fc->v[(i + 1) % fc->n];
			gfx_line(sx[p], sy[p], sx[q], sy[q], !solid);
		}
	}

	gfx_fill(0, 0, GFX_W, 9, 0);
	ui_title("3D", m->name);
}

const ui_screen_t scr_cube = { "3D solids", cube_enter, cube_key, cube_frame, 0 };
