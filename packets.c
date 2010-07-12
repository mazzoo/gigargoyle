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

#include <stdint.h>
#include <unistd.h>
#include <sys/time.h>
#include <stdlib.h>

#include "config.h"
#include "packets.h"
#include "fifo.h"
#include "gigargoyle.h"

void in_packet(pkt_t * p, uint32_t plen)
{
	if (plen < 8)
	{
		LOG("PKTS: WARNING: dropping very short packet (len %d)\n", plen);
		return; /* drop very short packets */
	}

	if (p->pkt_len < plen)
	{
		LOG("PKTS: WARNING: dropping short packet (%d < %d)\n",
		    p->pkt_len, plen
		   );
		return; /* drop short packets */
	}

	if ((p->hdr & PKT_MASK_VERSION) != VERSION)
	{
		LOG("PKTS: WARNING: dropping pkt with invalid version, hdr %x\n", p->hdr);
		return; /* drop wrong version packets */
	}

	if (p->pkt_len != plen)
		LOG("PKTS: FIXME: support padded packets (netcat)\n");

	switch(p->hdr & PKT_MASK_TYPE)
	{
		case PKT_TYPE_SET_SCREEN_BLK:
		case PKT_TYPE_SET_SCREEN_WHT:
		case PKT_TYPE_SET_SCREEN_RND_BW:
		case PKT_TYPE_SET_SCREEN_RND_COL:
		case PKT_TYPE_SET_FRAME_RATE:
		case PKT_TYPE_SET_FADE_RATE:
		case PKT_TYPE_SET_DURATION:
		case PKT_TYPE_SET_PIXEL:
		case PKT_TYPE_SET_SCREEN:
		case PKT_TYPE_FLIP_DBL_BUF:
		case PKT_TYPE_TEXT:
		case PKT_TYPE_SET_FONT:
			wr_fifo(p);
			break;
		/* out-of-band immediate commands follow */
		case PKT_TYPE_FLUSH_FIFO:
			flush_fifo();
			break;
		case PKT_TYPE_SHUTDOWN:
			gigargoyle_shutdown(); /* FIXME only from QM, not from IS */
			break;
		default:
			return; /* drop unsupported packages */
	}
}

void set_pixel_xy(
                  uint16_t r,
                  uint16_t g,
                  uint16_t b,
                  uint16_t x,
                  uint16_t y
                 ){
	static uint64_t last_timestamp[4] =
	                {0, 0, 0, 0}; /* FIXME flexible row/bus mapping */

	uint64_t       timestamp;

	uint8_t bus_buf[9];
	bus_buf[0] = 0x5c;
	bus_buf[1] = 0x30;
	bus_buf[2] = x + 0x10;
	bus_buf[3] = 'C';
	bus_buf[4] = r;
	bus_buf[5] = g;
	bus_buf[6] = b;
	bus_buf[7] = 0x5c;
	bus_buf[8] = 0x31;
	int ret;

	timestamp = gettimeofday64();

	if (last_timestamp[y] + MIN_GAP_BUS_TRANSFERS > timestamp)
		usleep(last_timestamp[y] + MIN_GAP_BUS_TRANSFERS - timestamp);

	ret = write(row[y], bus_buf, 9);
	if (ret != 9)
		LOG("PKTS: WARNING: write(bus %d) = %d != 9\n", y, ret);
	last_timestamp[y] = timestamp;
}

void set_screen_blk(void)
{
	int ix, iy;
	for (ix=0; ix < ACAB_X; ix++)
	{
		for (iy=0; iy < ACAB_Y; iy++)
		{
			set_pixel_xy(0, 0, 0, ix, iy);
		}
	}
}

void set_screen(uint8_t s[ACAB_X][ACAB_Y][3])
{
	int ix, iy;
	for (ix=0; ix < ACAB_X; ix++)
	{
		for (iy=0; iy < ACAB_Y; iy++)
		{
			set_pixel_xy(
			                s[ix][iy][0],
			                s[ix][iy][1],
			                s[ix][iy][2],
					ix, iy
				    );
		}
	}
}

void set_screen_rnd_bw(void)
{
	int ix, iy;
	for (ix=0; ix < ACAB_X; ix++)
	{
		for (iy=0; iy < ACAB_Y; iy++)
		{
			if (random()&1)
			{
				tmp_screen[ix][iy][0] = 0xff;
				tmp_screen[ix][iy][1] = 0xff;
				tmp_screen[ix][iy][2] = 0xff;
			}else{
				tmp_screen[ix][iy][0] = 0x00;
				tmp_screen[ix][iy][1] = 0x00;
				tmp_screen[ix][iy][2] = 0x00;
			}
		}
	}
	set_screen(tmp_screen);
}

void set_screen_rnd_col(void)
{
	int ix, iy;
	for (ix=0; ix < ACAB_X; ix++)
	{
		for (iy=0; iy < ACAB_Y; iy++)
		{
			tmp_screen[ix][iy][0] = random();
			tmp_screen[ix][iy][1] = random();
			tmp_screen[ix][iy][2] = random();
		}
	}
	set_screen(tmp_screen);
}

void next_frame(void)
{
	if (fifo_state == FIFO_EMPTY)
	{
		if (source == SOURCE_LOCAL)
			fill_fifo_local();
		return;
	}

	pkt_t * p;
	p = rd_fifo();

	switch(p->hdr & PKT_MASK_TYPE)
	{
		case PKT_TYPE_SET_SCREEN_BLK:
		case PKT_TYPE_SET_SCREEN_WHT:
		case PKT_TYPE_SET_SCREEN_RND_BW:
		case PKT_TYPE_SET_SCREEN_RND_COL:
		case PKT_TYPE_SET_FADE_RATE:
		case PKT_TYPE_SET_PIXEL:
		case PKT_TYPE_SET_SCREEN:
		case PKT_TYPE_FLIP_DBL_BUF:
		case PKT_TYPE_TEXT:
		case PKT_TYPE_SET_FONT:
			break;
		case PKT_TYPE_SET_FRAME_RATE:
			if (p->pkt_len < 64)
			{
				LOG("PKTS: WARNING: dropping short SET_FRAME_RATE pkt\n");
				return;
			}
			frame_duration = 1000000 / (*((uint32_t *)p->data));
			break;
		case PKT_TYPE_SET_DURATION:
			if (p->pkt_len < 64)
			{
				LOG("PKTS: WARNING: dropping short SET_DURATION pkt\n");
				return;
			}
			frame_duration = *((uint32_t *)p->data);
			break;

		/* out-of-band immediate commands follow */
		/* but were handled already async when entering gigargoyle */
		case PKT_TYPE_FLUSH_FIFO:
		case PKT_TYPE_SHUTDOWN:
		default:
			return; /* drop unsupported packages */
	}
}

