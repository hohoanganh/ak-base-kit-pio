#include <stdio.h>
#include <string.h>
#include "fake_link.h"

#define Q_SIZE 1024
typedef struct { uint8_t d[Q_SIZE]; uint16_t head, tail; } queue_t;

static queue_t c2s, s2c;			/* client->server, server->client */
static nmbs_t* attached_server;
static int corrupt_next;
int fake_failures;

static void q_push(queue_t* q, const uint8_t* b, uint16_t n) {
	for (uint16_t i = 0; i < n; i++) { q->d[q->head] = b[i]; q->head = (q->head + 1) % Q_SIZE; }
}
static uint16_t q_pop(queue_t* q, uint8_t* b, uint16_t n) {
	uint16_t k = 0;
	while (k < n && q->tail != q->head) { b[k++] = q->d[q->tail]; q->tail = (q->tail + 1) % Q_SIZE; }
	return k;
}

void fake_link_reset(void) { memset(&c2s, 0, sizeof c2s); memset(&s2c, 0, sizeof s2c); attached_server = NULL; corrupt_next = 0; }
void fake_link_attach_server(nmbs_t* server) { attached_server = server; }
void fake_link_corrupt_next_response(void) { corrupt_next = 1; }

int32_t fake_client_read(uint8_t* buf, uint16_t count, int32_t t, void* arg) {
	(void)t; (void)arg;
	uint16_t n = q_pop(&s2c, buf, count);
	if (n < count && attached_server && c2s.tail != c2s.head) {
		nmbs_server_poll(attached_server);
		n += q_pop(&s2c, buf + n, count - n);
	}
	return n;
}
int32_t fake_client_write(const uint8_t* buf, uint16_t count, int32_t t, void* arg) {
	(void)t; (void)arg; q_push(&c2s, buf, count); return count;
}
int32_t fake_server_read(uint8_t* buf, uint16_t count, int32_t t, void* arg) {
	(void)t; (void)arg; return q_pop(&c2s, buf, count);
}
int32_t fake_server_write(const uint8_t* buf, uint16_t count, int32_t t, void* arg) {
	(void)t; (void)arg;
	uint8_t tmp[300];
	memcpy(tmp, buf, count);
	if (corrupt_next && count > 0) { tmp[count - 1] ^= 0x01; corrupt_next = 0; }
	q_push(&s2c, tmp, count);
	return count;
}

void fake_make_client(nmbs_t* client, uint8_t dest) {
	nmbs_platform_conf conf;
	nmbs_platform_conf_create(&conf);
	conf.transport = NMBS_TRANSPORT_RTU;
	conf.read = fake_client_read;
	conf.write = fake_client_write;
	nmbs_client_create(client, &conf);
	nmbs_set_destination_rtu_address(client, dest);
	nmbs_set_read_timeout(client, 100);
	nmbs_set_byte_timeout(client, 20);
}
void fake_make_server(nmbs_t* server, uint8_t address, const nmbs_callbacks* cb) {
	nmbs_platform_conf conf;
	nmbs_platform_conf_create(&conf);
	conf.transport = NMBS_TRANSPORT_RTU;
	conf.read = fake_server_read;
	conf.write = fake_server_write;
	nmbs_server_create(server, address, &conf, cb);
	nmbs_set_read_timeout(server, 0);
	nmbs_set_byte_timeout(server, 20);
	fake_link_attach_server(server);
}
