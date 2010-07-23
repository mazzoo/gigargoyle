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

uint8_t source;           /* changed when QM or IS data come in
                           * or fifo runs empty */

uint32_t frame_duration;  /* us per frame, modified by
                           * PKT_TYPE_SET_FRAME_RATE or
                           * PKT_TYPE_SET_DURATION */

int row[4];               /* file handles for the uarts */
int * web;                /* file handle for web clients accept()ed */

uint8_t tmp_screen8 [ACAB_Y][ACAB_X][3];
uint8_t tmp_screen16[ACAB_X][ACAB_Y][6];

uint64_t gettimeofday64(void);

uint8_t get_source(void);

void gigargoyle_shutdown(void);

#endif /* GIGARGOYLE_H */
