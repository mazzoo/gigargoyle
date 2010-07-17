#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <errno.h>

#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netdb.h>

#include "packets.h"

#include "gg_simple_client.h"

gg_frame *gg_init_frame(unsigned int cols,
                        unsigned int rows,
                        unsigned int depth) {
  gg_frame *f;
  f = (gg_frame *)malloc(sizeof(gg_frame));

  if (f == NULL) {
    fprintf(stderr, "Error: Could not allocate frame");
    exit(-1);
  }

  f->rows = rows;
  f->cols = cols;
  f->depth = depth;

  f->packet = create_packet(VERSION,
                            PKT_MASK_DBL_BUF | PKT_MASK_RGB8,
                            PKT_TYPE_SET_SCREEN,
                            cols, rows, depth);

  if (f->packet == NULL) {
    fprintf(stderr, "Error: Could not allocate frame");
    exit(-1);
  }

  return f;
}

void gg_deinit_frame(gg_frame *f) {
  free(f->packet->data);
  free(f->packet);
  free(f);
}

gg_socket *gg_init_socket(const char *host, int port) {
  int ret;
  gg_socket *sock;
  struct sockaddr_in sa;
  struct hostent *adr;

  sock = (gg_socket *)malloc(sizeof(gg_socket));
  if (sock == NULL) {
    fprintf(stderr, "ERROR: allocate socket: %s\n", strerror(errno));
    exit(1);
  }

  int s = socket(AF_INET, SOCK_STREAM, 0);
  if (s < 0)
  {
    fprintf(stderr, "ERROR: connect(): %s\n", strerror(errno));
    exit(1);
  }

  memset(&sa, 0, sizeof(sa));
  adr = gethostbyname(host);
  
  if (adr) {
    memcpy(&sa.sin_addr, adr->h_addr, adr->h_length);
  } else if ( (sa.sin_addr.s_addr = inet_addr(host)) == INADDR_NONE ) {
    fprintf(stderr, "Can't get \"%s\" host entry\n", host);
    exit(-1);
  }
  
  sa.sin_family        = AF_INET;
  sa.sin_port          = htons(port);

  ret = connect(s, (struct sockaddr *) &sa, sizeof(sa));

  if (ret < 0)
  {
    fprintf(stderr, "ERROR: connect(): %s\n", strerror(errno));
    exit(1);
  }

  sock->s = s;

  return sock;
}

int gg_deinit_socket(gg_socket *s) {
  return  1/* close(s->s) */;
}

pkt_t *create_packet(unsigned int version,
                     uint32_t options,
                     uint8_t opcode,
                     unsigned int cols,
                     unsigned int rows,
                     unsigned int depth) {
  pkt_t *p;
  
  unsigned int payload_len = 0;

  switch (opcode) {
  case PKT_TYPE_SET_FRAME_RATE:
  case PKT_TYPE_SET_FADE_RATE:
  case PKT_TYPE_SET_DURATION:
    payload_len = 4;
    break;
  case PKT_TYPE_SET_SCREEN:
    payload_len = rows * cols * depth;
    break;
  }

  p = (pkt_t *)malloc(sizeof(pkt_t));

  p->hdr = VERSION << VERSION_SHIFT;
  p->hdr |= options;
  p->hdr |= opcode;

  p->data = (uint8_t *)malloc(payload_len);

  p->pkt_len = 8 + payload_len;

  return p;
}

uint8_t *serialize_packet(pkt_t *p) {
  uint8_t *packet;
  packet = (uint8_t *)malloc(p->pkt_len);

  uint32_t *packet_hdr = (uint32_t *)packet;
  *packet_hdr = htonl(p->hdr);

  uint32_t *packet_len = (uint32_t *)(packet+4);
  *packet_len = htonl(p->pkt_len);

  if (p->pkt_len > 8)
    memcpy(packet+8, p->data, p->pkt_len-8);

  return packet;
}

void gg_set_duration(gg_socket *s, unsigned int duration) {
  pkt_t *p;

  uint8_t *packet;

  p = create_packet(VERSION,
                    PKT_MASK_DBL_BUF | PKT_MASK_RGB8,
                    PKT_TYPE_SET_DURATION,
                    0, 0, 0);
  
  packet = serialize_packet(p);

  int off = 0;
  int ret = 0;
  while(ret+off < p->pkt_len) {
    off += ret;
    ret = write(s->s, packet+off, p->pkt_len-off);
  }

  free(p->data);
  free(p);
  free(packet);
}

void gg_set_pixel_color(gg_frame *f,
                        unsigned int col,
                        unsigned int row,
                        unsigned char r,
                        unsigned char g,
                        unsigned char b) {
  uint8_t *data = f->packet->data;

  *(data + row*f->cols*f->depth + col*f->depth + 0) = r;
  *(data + row*f->cols*f->depth + col*f->depth + 1) = g;
  *(data + row*f->cols*f->depth + col*f->depth + 2) = b;
}

void gg_set_frame_color(gg_frame *f,
                        unsigned char r,
                        unsigned char g,
                        unsigned char b) {
  int col, row;
  
  for (col = 0; col < f->cols; ++col) {
    for (row = 0; row < f->rows; ++row) {
      gg_set_pixel_color(f, row, col, r, g, b);
    }
  }
}

void gg_send_command(gg_socket *s, uint8_t opcode) {
  int ret;
  pkt_t *p;
  uint8_t *packet;

  p = create_packet(VERSION,
                    PKT_MASK_DBL_BUF | PKT_MASK_RGB8,
                    opcode,
                    0, 0, 0);
  
  packet = serialize_packet(p);

  ret = write(s->s, packet, p->pkt_len);
  if (ret != p->pkt_len) {
    fprintf(stderr, "Warning: Could not send packet\n");
  }
}

void gg_send_frame(gg_socket *s, gg_frame *f) {
  int ret;
  uint8_t *packet;
  pkt_t *dblbuf;
  
  /* Send data */
  packet = serialize_packet(f->packet);

  int off = 0;
  ret = 0;
  while(ret+off < f->packet->pkt_len) {
    off += ret;
    ret = write(s->s, packet+off, f->packet->pkt_len-off);
  }

  free(packet);

  /* Send flip command */
  /*dblbuf = create_packet(VERSION,
                    PKT_MASK_DBL_BUF | PKT_MASK_RGB8,
                    PKT_TYPE_FLIP_DBL_BUF,
                    0, 0, 0);
  
  packet = serialize_packet(dblbuf);

  off = 0;
  ret = 0;
  while(ret+off < dblbuf->pkt_len) {
    off += ret;
    ret = write(s->s, packet+off, dblbuf->pkt_len-off);
  }*/

  /* Free used structs */
  //free(dblbuf->data);
  //free(dblbuf);
  //free(packet);
}
