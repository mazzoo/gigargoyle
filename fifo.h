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

#ifndef FIFO_H
#define FIFO_H

#include "config.h"
#include "packets.h"
#include "fifo.h"
#include "gigargoyle.h"

#define FIFO_EMPTY 0x1
#define FIFO_HALF  0x2
#define FIFO_FULL  0x3
                      
uint8_t ** fifo;
uint32_t   fifo_rd;
uint32_t   fifo_wr;
int        fifo_state;

/* local buffer for a single fifo pkt */
pkt_t * fp;

pkt_t * rd_fifo(void);
void wr_fifo(pkt_t * p);
void flush_fifo(void);
void init_fifo(void);
void fill_fifo_local(void);

#endif /* FIFO_H */
