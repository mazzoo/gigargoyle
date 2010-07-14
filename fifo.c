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

#include <string.h>
#include <stdlib.h>

#include "fifo.h"

void wr_fifo(pkt_t * pkt)
{
	if (fifo_state == FIFO_FULL)
	{
		LOG("FIFO: WARNING: fifo full, dropping packet, hdr %x\n", pkt->hdr);
		return;
	}
	if (fifo_state == FIFO_FULL)
		fifo_state = FIFO_HALF;

	if (pkt->pkt_len > FIFO_WIDTH)
	{
		LOG("FIFO: WARNING: dropping long packet, hdr %x\n", pkt->hdr);
		return;
	}
	memcpy(fifo[fifo_wr], pkt, pkt->pkt_len);
	fifo_wr++;
	fifo_wr %= FIFO_DEPTH;
	if (fifo_wr == fifo_rd)
		fifo_state = FIFO_FULL;
	//LOG("FIFO[%4.4d:%4.4d]: stored pkt type %8.8x\n", fifo_rd, fifo_wr, pkt->hdr);
}

pkt_t * rd_fifo(void)
{
	static int running_empty_on_network = 0;
	if (fifo_state == FIFO_EMPTY)
	{
		LOG("FIFO: WARNING: fifo empty, returning NULL\n");
		if (source == SOURCE_LOCAL)
			fill_fifo_local();
		else
			running_empty_on_network++;

		if (running_empty_on_network >= MISSING_PKTS_TO_LOCAL)
		{
			LOG("FIFO: WARNING: filling fifo with local data");
			LOG("as we're running empty from the network\n");
			fill_fifo_local();
			running_empty_on_network = 0;
		}

		return NULL;
	}

	pkt_t * p = (pkt_t *) fifo[fifo_rd];
	memcpy(fp, fifo[fifo_rd], p->pkt_len);

	fifo_rd++;
	fifo_rd %= FIFO_DEPTH;
	if (fifo_wr == fifo_rd)
		fifo_state = FIFO_EMPTY;

	return fp;
}

void flush_fifo(void)
{
	fifo_rd = 0;
	fifo_wr = 0;
	fifo_state = FIFO_EMPTY;
	LOG("FIFO: flushing\n");
}

void init_fifo(void)
{
	fifo = malloc(FIFO_DEPTH * (sizeof(fifo)));
	if (!fifo)
	{
		LOG("ERROR: out of memory (fifo)\n");
		exit(1);
	}

	int i;
	for (i=0; i<FIFO_DEPTH; i++)
	{
		fifo[i] = malloc(FIFO_WIDTH);
		if (!fifo[i])
		{
			LOG("ERROR: out of memory (fifo %d)\n", i);
			exit(1);
		}
	}
	fifo_rd    = 0;
	fifo_wr    = 0;
	fifo_state = FIFO_EMPTY;

	fp = malloc(sizeof(pkt_t) + FIFO_WIDTH);
	if (!fp)
	{
		LOG("ERROR: out of memory (fp)\n");
		exit(1);
	}
	fill_fifo_local();
}

void fill_fifo_local(void)
{
	LOG("FIFO: filling fifo with local animation\n");
	fp->pkt_len = 8; /* header only, no payload */
	fp->hdr = VERSION << VERSION_SHIFT; /* header               */
	fp->hdr |= PKT_MASK_DBL_BUF;        /* as we use it for all */
	fp->hdr |= PKT_MASK_RGB8;           /* following packets    */

	fp->hdr |= PKT_TYPE_SET_SCREEN_BLK; /* command: black screen */

	wr_fifo(fp);

	fp->hdr &= ~PKT_MASK_TYPE;          /* clear command */
	fp->hdr |= PKT_TYPE_SET_SCREEN_RND_BW; /* command: random bw screen */

	int i;
	for (i=1; i < FIFO_DEPTH; i++)
	{
		wr_fifo(fp);
	}
}
