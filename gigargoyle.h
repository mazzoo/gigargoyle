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

#ifndef GIGARGOYLE_H
#define GIGARGOYLE_H

#include <stdio.h>

/* logging */
int logfd;    /* logfile descriptor */
FILE * logfp;
#define LOG(fmt, args...) {fprintf(logfp, fmt, ##args); fflush(logfp);}

int row[4]; /* file handles for the uarts */

uint8_t tmp_screen[ACAB_X][ACAB_Y][3];

/* fifo */
#define FIFO_EMPTY 0x1
#define FIFO_HALF  0x2
#define FIFO_FULL  0x3
                      
pkt_t * rd_fifo(void);
void wr_fifo(pkt_t * p);
void flush_fifo(void);

uint64_t gettimeofday64(void);

uint8_t get_source(void);

void gigargoyle_shutdown(void);

#endif /* GIGARGOYLE_H */
