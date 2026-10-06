/**
 ******************************************************************************
 * @brief:  Player for 1-bit video clips kept in the media store (SPI NOR).
 *          Clips are made on the PC with tools/ak_video.py.
 *
 *  File (.akv), little endian:
 *    header, 16 bytes: "AKV1", width (128), height (64), frames per second,
 *                      0, number of frames (u16), 0 (u16), data length (u32)
 *    then one record per frame:
 *      changed  1 byte   bit n set = display page n is in this record
 *      xor      1 byte   bit n set = the data of page n is XORed onto the
 *                        page on screen, else it replaces it
 *      for each changed page, lowest first: PackBits data for its 128 bytes
 *        control c < 128:  the next c + 1 bytes as they are
 *        control c >= 128: the next byte, c - 126 times (2..129)
 *  The first frame carries all pages without XOR, so playing can restart
 *  there at any time.
 *
 *  The decoder works straight on the frame buffer and reads the store in
 *  small pieces: no frame-sized buffer in RAM.
 ******************************************************************************
**/

#ifndef __VIDEO_H__
#define __VIDEO_H__

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdint.h>

#define VIDEO_HEADER_SIZE		(16)
#define VIDEO_PAGES				(8)
#define VIDEO_PAGE_BYTES		(128)

/* Reads len bytes at offset off of the store. Returns 1 on success. */
typedef uint8_t (*video_read_t)(uint32_t off, void* buf, uint32_t len);

/* Looks for a clip at the start of a store of store_size bytes.
 * Returns 1 if there is one that fits. */
extern uint8_t video_open(video_read_t read, uint32_t store_size);

extern uint16_t video_frames(void);
extern uint8_t video_fps(void);
extern uint32_t video_size(void);			/* header + data, bytes */
extern uint16_t video_pos(void);			/* frames decoded since the start */

extern void video_rewind(void);

/* Decodes the next frame into fb ([page][column], bit 0 = top pixel).
 * Returns 1 on success, 0 at the end of the clip or on damaged data. */
extern uint8_t video_next(uint8_t* fb);

#ifdef __cplusplus
}
#endif

#endif /* __VIDEO_H__ */
