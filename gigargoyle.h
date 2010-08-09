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

FILE * logfp;
#define LOG(fmt, args...) {fprintf(logfp, fmt, ##args); fflush(logfp);}

typedef struct streamingsource_s
{
	int       listener;       /* file handle for the QM or IS listen()   */
	int       sock;           /* file handle for the QM or IS accept()ed */
	int       state;

	int       input_offset;
	uint8_t * buf;
} streamingsource_t;

typedef struct web_s
{
	int listener;              /* file handle for web clients listen()   */
	int * sock;                /* file handle for web clients accept()ed */
	int state;
} web_t;

typedef struct fifo_s
{
	uint8_t ** fifo;
	uint32_t   rd;
	uint32_t   wr;
	int        state;

} fifo_t;

typedef struct gigargoyle_s
{
	int     logfd;            /* logfile descriptor */

	pid_t   daemon_pid;

	uint8_t source;           /* one of SOURCE_LOCAL, SOURCE_QM, SOURCE_IS
	                           * changed when QM or IS data come in
	                           * or fifo runs empty */

	streamingsource_t * qm;   /* queuing manager */
	streamingsource_t * is;   /* instant streaming */
	streamingsource_t * ss;   /* actual streaming source. either is NULL,
	                             or points to qm or is */

	fifo_t            * fifo;

	web_t             * web;  /* WEB clients structure */
} gigargoyle_t;

gigargoyle_t * ggg;


uint32_t frame_duration;  /* us per frame, modified by
                           * PKT_TYPE_SET_FRAME_RATE or
                           * PKT_TYPE_SET_DURATION */

int row[4];               /* file handles for the uarts */

uint8_t tmp_screen8 [ACAB_Y][ACAB_X][3];
uint8_t tmp_screen16[ACAB_X][ACAB_Y][6];

uint64_t gettimeofday64(void);

uint8_t get_source(void);

void gigargoyle_shutdown(void);

#endif /* GIGARGOYLE_H */
