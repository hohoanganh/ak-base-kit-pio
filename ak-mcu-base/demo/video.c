#include <string.h>

#include "video.h"

#define CACHE_SIZE		(64)

static video_read_t rd;
static uint16_t frames_total;
static uint8_t fps;
static uint32_t data_len;			/* bytes after the header */
static uint32_t offset;				/* next byte to decode, counted after the header */
static uint16_t pos;

static uint8_t cache[CACHE_SIZE];
static uint32_t cache_off;
static uint8_t cache_len;

/* Next byte of the clip; 0 when the data is used up or the store fails. */
static uint8_t get(uint8_t* v) {
	if (offset >= data_len) {
		return 0;
	}
	if (offset < cache_off || offset >= cache_off + cache_len) {
		uint32_t n = data_len - offset;

		if (n > CACHE_SIZE) {
			n = CACHE_SIZE;
		}
		if (!rd(VIDEO_HEADER_SIZE + offset, cache, n)) {
			cache_len = 0;
			return 0;
		}
		cache_off = offset;
		cache_len = (uint8_t)n;
	}
	*v = cache[offset - cache_off];
	offset++;
	return 1;
}

uint8_t video_open(video_read_t read, uint32_t store_size) {
	uint8_t h[VIDEO_HEADER_SIZE];

	rd = 0;
	frames_total = 0;
	if (!read || store_size < VIDEO_HEADER_SIZE || !read(0, h, sizeof(h))) {
		return 0;
	}
	if (memcmp(h, "AKV1", 4) != 0 || h[4] != VIDEO_PAGE_BYTES || h[5] != VIDEO_PAGES * 8 || h[6] == 0) {
		return 0;
	}
	data_len = (uint32_t)h[12] | ((uint32_t)h[13] << 8) | ((uint32_t)h[14] << 16) | ((uint32_t)h[15] << 24);
	if (data_len > store_size - VIDEO_HEADER_SIZE) {
		return 0;
	}
	rd = read;
	fps = h[6];
	frames_total = (uint16_t)(h[8] | (h[9] << 8));
	video_rewind();
	return frames_total != 0;
}

uint16_t video_frames(void) {
	return frames_total;
}

uint8_t video_fps(void) {
	return fps;
}

uint32_t video_size(void) {
	return frames_total ? VIDEO_HEADER_SIZE + data_len : 0;
}

uint16_t video_pos(void) {
	return pos;
}

void video_rewind(void) {
	offset = 0;
	pos = 0;
	cache_len = 0;
}

uint8_t video_next(uint8_t* fb) {
	uint8_t changed, xor_mask;

	if (!rd || pos >= frames_total || !get(&changed) || !get(&xor_mask)) {
		return 0;
	}
	for (uint8_t page = 0; page < VIDEO_PAGES; page++) {
		uint8_t* p = fb + (uint16_t)page * VIDEO_PAGE_BYTES;
		uint8_t use_xor = (uint8_t)((xor_mask >> page) & 1);
		uint16_t done = 0;

		if (!((changed >> page) & 1)) {
			continue;
		}
		while (done < VIDEO_PAGE_BYTES) {
			uint8_t c, v = 0;
			uint16_t n;

			if (!get(&c)) {
				return 0;
			}
			n = (c < 128) ? (uint16_t)(c + 1) : (uint16_t)(c - 126);
			if (n > VIDEO_PAGE_BYTES - done) {
				return 0;				/* damaged: would run past the page */
			}
			if (c >= 128 && !get(&v)) {
				return 0;
			}
			while (n--) {
				if (c < 128 && !get(&v)) {
					return 0;
				}
				if (use_xor) {
					p[done] ^= v;
				}
				else {
					p[done] = v;
				}
				done++;
			}
		}
	}
	pos++;
	return 1;
}
