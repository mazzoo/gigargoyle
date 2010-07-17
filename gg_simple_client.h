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
 *          Raffael Mancini <raffael.mancini@hcl-club.lu>
 *
 */

#ifndef GG_SIMPLE_CLIENT_H
#define GG_SIMPLE_CLIENT_H

#include <stdint.h>
#include <sys/socket.h>

#include "packets.h"

typedef struct {
  unsigned int cols;
  unsigned int rows;
  unsigned int depth;

  pkt_t *packet;
} gg_frame;

typedef struct {
  int s;
} gg_socket;

/* Initialize the frame structure */
gg_frame *gg_init_frame(unsigned int rows,
                        unsigned int cols,
                        unsigned int depth);
/* Free the frame structure */
void gg_deinit_frame(gg_frame *f);

/* Initialize and connect the socket */
gg_socket *gg_init_socket(const char *host, int port);
/* Disconnect and realease socket */
int gg_deinit_socket(gg_socket *s);

/* Set the duration in us of a frame to be displayed */
void gg_set_duration(gg_socket *f, unsigned int duration);

/* Set 24bit color of pixel at location (row, col) */
void gg_set_pixel_color(gg_frame *f,
                        unsigned int col,
                        unsigned int row,
                        unsigned char r,
                        unsigned char g,
                        unsigned char b);
/* Clear the color of the frame to (r, g, b) */
void gg_set_frame_color(gg_frame *f,
                        unsigned char r,
                        unsigned char g,
                        unsigned char b);
/* Send a command without payload*/
void gg_send_command(gg_socket *s, uint8_t opcode);

/* Send a frame and flip buffer */
void gg_send_frame(gg_socket *s, gg_frame *f);

pkt_t *create_packet(unsigned int version,
                     uint32_t options,
                     uint8_t opcode,
                     unsigned int cols,
                     unsigned int rows,
                     unsigned int depth);

uint8_t *serialize_packet(pkt_t *p);
void send_packet(gg_socket *s, pkt_t *packet);

#endif
