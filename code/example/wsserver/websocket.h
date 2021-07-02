
#ifndef WEBSOCKET_H
#define WEBSOCKET_H
#if defined(WIN32)

#include <winsock.h>

#else
#include <netinet/in.h>
#endif

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>

#include <string.h>
#include <stdio.h>
#include <ctype.h>


#ifdef __AVR__
#include <avr/pgmspace.h>
#else
#define PROGMEM
#define PSTR
#define strstr_P strstr
#define sscanf_P sscanf
#define sprintf_P sprintf
#define strlen_P strlen
#define memcmp_P memcmp
#define memcpy_P memcpy
#endif

#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif

static const char connectionField[] PROGMEM = "Connection: ";
static const char upgrade[] PROGMEM = "upgrade";
static const char upgrade2[] PROGMEM = "Upgrade";
static const char upgradeField[] PROGMEM = "Upgrade: ";
static const char websocket[] PROGMEM = "websocket";
static const char hostField[] PROGMEM = "Host: ";
static const char originField[] PROGMEM = "Origin: ";
static const char keyField[] PROGMEM = "Sec-WebSocket-Key: ";
static const char protocolField[] PROGMEM = "Sec-WebSocket-Protocol: ";
static const char versionField[] PROGMEM = "Sec-WebSocket-Version: ";
static const char version[] PROGMEM = "13";
static const char secret[] PROGMEM = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

enum wsFrameType {
	WS_EMPTY_FRAME = 0xF0,
	WS_ERROR_FRAME = 0xF1,
	WS_INCOMPLETE_FRAME = 0xF2,
	WS_CONTINUATION_FRAME = 0x0,
	WS_TEXT_FRAME = 0x01,
	WS_BINARY_FRAME = 0x02,
	WS_PING_FRAME = 0x09,
	WS_PONG_FRAME = 0x0A,
	WS_OPENING_FRAME = 0xF3,
	WS_CLOSING_FRAME = 0x08
};

enum wsState {
	WS_STATE_OPENING,
	WS_STATE_NORMAL,
	WS_STATE_CLOSING
};

struct handshake {
	char *host;
	char *origin;
	char *key;
	char *resource;
	enum wsFrameType frameType;
};


#include <string.h>

#if _MSC_VER
#define _sha1_restrict __restrict
#else
#define _sha1_restrict __restrict__
#endif

#define SHA1_SIZE 20

static inline void sha1mix(unsigned *_sha1_restrict r, unsigned *_sha1_restrict w) {
	unsigned a = r[0];
	unsigned b = r[1];
	unsigned c = r[2];
	unsigned d = r[3];
	unsigned e = r[4];
	unsigned t, i = 0;

#define rol(x, s) ((x) << (s) | (unsigned)(x) >> (32 - (s)))
#define mix(f, v)                                   \
    do                                              \
    {                                               \
        t = (f) + (v) + rol(a, 5) + e + w[i & 0xf]; \
        e = d;                                      \
        d = c;                                      \
        c = rol(b, 30);                             \
        b = a;                                      \
        a = t;                                      \
    } while (0)

	for (; i < 16; ++i)
		mix(d ^ (b & (c ^ d)), 0x5a827999);

	for (; i < 20; ++i) {
		w[i & 0xf] = rol(w[i + 13 & 0xf] ^ w[i + 8 & 0xf] ^ w[i + 2 & 0xf] ^ w[i & 0xf], 1);
		mix(d ^ (b & (c ^ d)), 0x5a827999);
	}

	for (; i < 40; ++i) {
		w[i & 0xf] = rol(w[i + 13 & 0xf] ^ w[i + 8 & 0xf] ^ w[i + 2 & 0xf] ^ w[i & 0xf], 1);
		mix(b ^ c ^ d, 0x6ed9eba1);
	}

	for (; i < 60; ++i) {
		w[i & 0xf] = rol(w[i + 13 & 0xf] ^ w[i + 8 & 0xf] ^ w[i + 2 & 0xf] ^ w[i & 0xf], 1);
		mix((b & c) | (d & (b | c)), 0x8f1bbcdc);
	}

	for (; i < 80; ++i) {
		w[i & 0xf] = rol(w[i + 13 & 0xf] ^ w[i + 8 & 0xf] ^ w[i + 2 & 0xf] ^ w[i & 0xf], 1);
		mix(b ^ c ^ d, 0xca62c1d6);
	}

#undef mix
#undef rol

	r[0] += a;
	r[1] += b;
	r[2] += c;
	r[3] += d;
	r[4] += e;
}

static void sha1(unsigned char h[SHA1_SIZE], const void *_sha1_restrict p, size_t n) {
	size_t i = 0;
	unsigned w[16], r[5] = {0x67452301, 0xefcdab89, 0x98badcfe, 0x10325476, 0xc3d2e1f0};

	for (; i < (n & ~0x3f);) {
		do
			w[i >> 2 & 0xf] =
					((const unsigned char *) p)[i + 3] << 0x00 |
					((const unsigned char *) p)[i + 2] << 0x08 |
					((const unsigned char *) p)[i + 1] << 0x10 |
					((const unsigned char *) p)[i + 0] << 0x18;
		while ((i += 4) & 0x3f);
		sha1mix(r, w);
	}

	memset(w, 0, sizeof w);

	for (; i < n; ++i)
		w[i >> 2 & 0xf] |= ((const unsigned char *) p)[i] << ((3 ^ (i & 3)) << 3);

	w[i >> 2 & 0xf] |= 0x80 << ((3 ^ (i & 3)) << 3);

	if ((n & 0x3f) > 56) {
		sha1mix(r, w);
		memset(w, 0, sizeof w);
	}

	w[15] = n << 3;
	sha1mix(r, w);

	for (i = 0; i < 5; ++i)
		h[(i << 2) + 0] = (unsigned char) (r[i] >> 0x18),
		h[(i << 2) + 1] = (unsigned char) (r[i] >> 0x10),
		h[(i << 2) + 2] = (unsigned char) (r[i] >> 0x08),
		h[(i << 2) + 3] = (unsigned char) (r[i] >> 0x00);
}

static inline size_t base64len(size_t n) {
	return (n + 2) / 3 * 4;
}

static size_t base64(char *buf, size_t nbuf, const unsigned char *p, size_t n) {
	const char t[64] = {'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P', 'Q', 'R', 'S',
						'T', 'U', 'V', 'W', 'X', 'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l',
						'm', 'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z', '0', '1', '2', '3', '4',
						'5', '6', '7', '8', '9', '+', '/'};
	size_t i, m = base64len(n);
	unsigned x;

	if (nbuf >= m)
		for (i = 0; i < n; ++i) {
			x = p[i] << 0x10;
			x |= (++i < n ? p[i] : 0) << 0x08;
			x |= (++i < n ? p[i] : 0) << 0x00;

			*buf++ = t[x >> 3 * 6 & 0x3f];
			*buf++ = t[x >> 2 * 6 & 0x3f];
			*buf++ = (((n - 0 - i) >> 31) & '=') |
					 (~((n - 0 - i) >> 31) & t[x >> 1 * 6 & 0x3f]);
			*buf++ = (((n - 1 - i) >> 31) & '=') |
					 (~((n - 1 - i) >> 31) & t[x >> 0 * 6 & 0x3f]);
		}

	return m;
}

enum wsFrameType wsParseHandshake(const uint8_t *inputFrame, size_t inputLength,
								  struct handshake *hs);

void wsGetHandshakeAnswer(const struct handshake *hs, uint8_t *outFrame,
						  size_t *outLength);

void wsMakeFrame(const uint8_t *data, size_t dataLength,
				 uint8_t *outFrame, size_t *outLength, enum wsFrameType frameType);

enum wsFrameType wsParseInputFrame(uint8_t *inputFrame, size_t inputLength,
								   uint8_t **dataPtr, size_t *dataLength);

void nullHandshake(struct handshake *hs);

void freeHandshake(struct handshake *hs);


#endif    /* WEBSOCKET_H */
