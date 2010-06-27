#ifndef PACKETS_H
#define PACKETS_H

#define PKT_MASK_VERSION 0xff000000 /* 0x00 */
#define PKT_MASK_DBL_BUF 0x00008000 /* uses feature double buffering    */
#define PKT_MASK_FADING  0x00004000 /* uses feature fading              */
#define PKT_MASK_RGB8    0x00002000 /* r8g8b8 format                    */
#define PKT_MASK_GREY4   0x00001000 /* 4bit grey per pixel              */
#define PKT_MASK_BW      0x00000800 /* bw 1 byte per pixel format       */
#define PKT_MASK_BW_PACK 0x00000400 /* bw pixel 8 pixel per byte format */
#define PKT_MASK_TYPE    0x000000ff

#define PKT_TYPE_SET_SCREEN_BLK 0x00
#define PKT_TYPE_SET_SCREEN_WHT 0x01

#define PKT_TYPE_SET_FRAME_RATE 0x08
#define PKT_TYPE_SET_FADE_RATE  0x09

#define PKT_TYPE_SET_PIXEL      0x10
#define PKT_TYPE_SET_SCREEN     0x11
#define PKT_TYPE_FLIP_DBL_BUF   0x12

typedef struct pkt_s {
	uint32_t  hdr;     /* see above definition */
	uint32_t  pkt_len; /* including header */
	uint8_t * data;    /* payload (if any) */
} pkt_t;

/* QM socket states */
#define QM_NOT_CONNECTED 0x01
#define QM_CONNECTED     0x02

/* IS socket states */
#define IS_NOT_CONNECTED 0x01
#define IS_CONNECTED     0x02

#endif /* PACKETS_H */
