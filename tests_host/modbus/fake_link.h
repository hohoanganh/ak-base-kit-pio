#ifndef FAKE_LINK_H
#define FAKE_LINK_H
/* Duong truyen RS485 gia: client va server nanoMODBUS trong CUNG tien trinh.
 * Khi client cho phan hoi ma hang doi rong, fake_client_read goi
 * nmbs_server_poll() mot lan de server xu ly request dang cho. */
#include <stdint.h>
#include "nanomodbus.h"

void fake_link_reset(void);
void fake_link_attach_server(nmbs_t* server);
void fake_link_corrupt_next_response(void);   /* lat 1 bit byte cuoi phan hoi ke tiep -> sai CRC */

int32_t fake_client_read(uint8_t* buf, uint16_t count, int32_t byte_timeout_ms, void* arg);
int32_t fake_client_write(const uint8_t* buf, uint16_t count, int32_t byte_timeout_ms, void* arg);
int32_t fake_server_read(uint8_t* buf, uint16_t count, int32_t byte_timeout_ms, void* arg);
int32_t fake_server_write(const uint8_t* buf, uint16_t count, int32_t byte_timeout_ms, void* arg);

void fake_make_client(nmbs_t* client, uint8_t dest);
void fake_make_server(nmbs_t* server, uint8_t address, const nmbs_callbacks* cb);

/* Assert toi gian: in dong loi va dem, main tra ma thoat != 0 neu co loi. */
extern int fake_failures;
#define CHECK(cond) do { if (!(cond)) { fake_failures++; \
	printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); } } while (0)
#endif
