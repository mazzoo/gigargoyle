/*
 * gigargoyle
 *
 * a nighttime composition in opensource
 *
 * this is part of the 2010 binkenlights installation
 * in Giesing, Munich, Germany which is called
 *
 *   a.c.a.b. - all colors are beautiful
 *
 * the installation is run by the Chaos Computer Club Munich
 * as part of the puerto giesing
 * 
 *
 * license:
 *          GPL v2, see the file LICENSE
 * authors:
 *          Matthias Wenzel - aka - mazzoo
 *
 */

#ifndef PACKETS_H
#define PACKETS_H

#include <stdint.h>
#include "config.h"

#define VERSION 0x00

#define VERSION_SHIFT      24
#define PKT_MASK_VERSION 0xff000000 /* 0x00 */
#define PKT_MASK_DBL_BUF 0x00008000 /* uses feature double buffering    */
#define PKT_MASK_FADING  0x00004000 /* uses feature fading              */
#define PKT_MASK_RGB16   0x00002000 /* r16g16b16 format                 */
#define PKT_MASK_RGB8    0x00001000 /* r8g8b8 format                    */
#define PKT_MASK_GREY4   0x00000800 /* 4bit grey per pixel              */
#define PKT_MASK_BW      0x00000400 /* bw 1 byte per pixel format       */
#define PKT_MASK_BW_PACK 0x00000200 /* bw pixel 8 pixel per byte format */
#define PKT_MASK_TYPE    0x000000ff

#define PKT_TYPE_SET_SCREEN_BLK     0x00
#define PKT_TYPE_SET_SCREEN_WHT     0x01
#define PKT_TYPE_SET_SCREEN_RND_BW  0x02
#define PKT_TYPE_SET_SCREEN_RND_COL 0x04

#define PKT_TYPE_SET_FRAME_RATE     0x08
#define PKT_TYPE_SET_FADE_RATE      0x09
#define PKT_TYPE_SET_DURATION       0x0a

#define PKT_TYPE_SET_PIXEL          0x10
#define PKT_TYPE_SET_SCREEN         0x11
#define PKT_TYPE_FLIP_DBL_BUF       0x12

#define PKT_TYPE_TEXT               0x20
#define PKT_TYPE_SET_FONT           0x21

#define PKT_TYPE_FLUSH_FIFO         0xfd

#define PKT_TYPE_SHUTDOWN           0xfe

typedef struct pkt_s {
	uint32_t  hdr;     /* see above definition */
	uint32_t  pkt_len; /* including header */
	uint8_t * data;    /* payload (if any) */
} pkt_t;

/* QM socket states (queuing manager) */
#define QM_NOT_CONNECTED 0x01
#define QM_CONNECTED     0x02
#define QM_ERROR         0x80

/* IS socket states (instant streamer) */
#define IS_NOT_CONNECTED 0x01
#define IS_CONNECTED     0x02

/* WEB socket states (instant streamer) */
#define WEB_NOT_CONNECTED 0x01
#define WEB_ERROR         0x80

/* gigargoyle streaming source */
#define SOURCE_QM        0x01
#define SOURCE_IS        0x02
#define SOURCE_LOCAL     0x80

/* prototypes from packets.c */
void in_packet(pkt_t * p, uint32_t plen);
void next_frame(void);
void serve_web_clients(void);

uint8_t shadow_screen[ACAB_X][ACAB_Y][3]; /* for web clients */

#endif /* PACKETS_H */
