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
#include "command_line_arguments.h"

/* Contains parsed command line arguments */
extern struct arguments arguments;

void in_packet(pkt_t * p, uint32_t plen)
{
	if (plen < 8)
	{
		LOG("PKTS: WARNING: dropping very short packet (len %d)\n", plen);
		return; /* drop very short packets */
	}

	if (plen < p->pkt_len )
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

void set_pixel_xy_rgb8(
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
	{
		usleep(last_timestamp[y] + MIN_GAP_BUS_TRANSFERS - timestamp);
		timestamp = gettimeofday64();
	}

	ret = write(row[y], bus_buf, 9);

	if (ret != 9)
		LOG("PKTS: WARNING: write(bus %d) = %d != 9\n", y, ret);
	last_timestamp[y] = timestamp;

	shadow_screen[x][y][0] = r;
	shadow_screen[x][y][1] = g;
	shadow_screen[x][y][2] = b;
}

void set_pixel_xy_rgb16(
                  uint16_t r,
                  uint16_t g,
                  uint16_t b,
                  uint16_t x,
                  uint16_t y
                 ){
	/* FIXME */
	set_pixel_xy_rgb8(r, g, b, x, y);
}

void set_screen_blk(void)
{
	int ix, iy;
	for (ix=0; ix < ACAB_X; ix++)
	{
		for (iy=0; iy < ACAB_Y; iy++)
		{
			set_pixel_xy_rgb8(0, 0, 0, ix, iy);
		}
	}
}

void set_screen_rgb8(uint8_t s[ACAB_X][ACAB_Y][3])
{
	int ix, iy;

	for (ix=0; ix < ACAB_X; ix++)
	{
		for (iy=0; iy < ACAB_Y; iy++)
		{
			set_pixel_xy_rgb8(
			                s[ix][iy][0],
			                s[ix][iy][1],
			                s[ix][iy][2],
					ix, iy
				    );
		}
	}
}

void set_screen_rgb16(uint16_t s[ACAB_X][ACAB_Y][3])
{
	int ix, iy;
	for (ix=0; ix < ACAB_X; ix++)
	{
		for (iy=0; iy < ACAB_Y; iy++)
		{
			set_pixel_xy_rgb16(
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
				tmp_screen8[ix][iy][0] = 0xff;
				tmp_screen8[ix][iy][1] = 0xff;
				tmp_screen8[ix][iy][2] = 0xff;
			}else{
				tmp_screen8[ix][iy][0] = 0x00;
				tmp_screen8[ix][iy][1] = 0x00;
				tmp_screen8[ix][iy][2] = 0x00;
			}
		}
	}
	set_screen_rgb8(tmp_screen8);
}

void set_screen_rnd_col(void)
{
	int ix, iy;
	for (ix=0; ix < ACAB_X; ix++)
	{
		for (iy=0; iy < ACAB_Y; iy++)
		{
			tmp_screen8[ix][iy][0] = random();
			tmp_screen8[ix][iy][1] = random();
			tmp_screen8[ix][iy][2] = random();
		}
	}
	set_screen_rgb8(tmp_screen8);
}

void flip_double_buffer_on_bus(int b)
{
	return;
	uint8_t bus_buf[9]; //FIXME
	bus_buf[0] = 0x5c;
	bus_buf[1] = 0x30;
	bus_buf[2] = 0x10;
	bus_buf[3] = 'C';
	bus_buf[7] = 0x5c;
	bus_buf[8] = 0x31;
	int ret;

	ret = write(row[b], bus_buf, 9);
	if (ret != 9)
		LOG("PKTS: WARNING: flip_double_buffer_on_bus() write(bus %d) = %d != 9\n", b, ret);
}

void flip_double_buffer(void)
{
	int i;
	for (i=0; i<4; i++)
		flip_double_buffer_on_bus(i);
}

void next_frame(void)
{
	pkt_t * p;
	p = rd_fifo();

	if (p == NULL)
	{
		LOG("PKTS: next_frame() ran into an empty fifo\n");
		return;
	}

	switch(p->hdr & PKT_MASK_TYPE)
	{
		case PKT_TYPE_SET_FADE_RATE:
		case PKT_TYPE_SET_PIXEL:
		case PKT_TYPE_FLIP_DBL_BUF:
		case PKT_TYPE_TEXT:
		case PKT_TYPE_SET_FONT:
		case PKT_TYPE_SET_SCREEN_BLK:
		case PKT_TYPE_SET_SCREEN_WHT:
		case PKT_TYPE_SET_SCREEN_RND_BW:
			set_screen_rnd_bw();
			break;
		case PKT_TYPE_SET_SCREEN_RND_COL:
			set_screen_rnd_col();
			break;

		case PKT_TYPE_SET_SCREEN:
			if (p->hdr & PKT_MASK_RGB16)
			{
				if (p->pkt_len != 8 + 2 * 3 * ACAB_X * ACAB_Y)
				{
					LOG("PKTS: WARNING: dropping SET_SCREEN RGB16 pkt, invalid length");
					LOG(" %d != %d\n", p->pkt_len, 8 + 2 * 3 * ACAB_X * ACAB_Y);
					return;
				}
				set_screen_rgb16((uint16_t (*)[ACAB_Y][3])p->data);
			}
			if (p->hdr & PKT_MASK_RGB8)
			{
				if (p->pkt_len != 8 + 3 * ACAB_X * ACAB_Y)
				{
					LOG("PKTS: WARNING: dropping SET_SCREEN RGB8 pkt, invalid length");
					LOG(" %d != %d\n", p->pkt_len, 8 + 3 * ACAB_X * ACAB_Y);
					return;
				}
				set_screen_rgb8((uint8_t (*)[ACAB_Y][3])p->data);
			}

			break;

		case PKT_TYPE_SET_FRAME_RATE:
			if (p->pkt_len != 12)
			{
				LOG("PKTS: WARNING: dropping short SET_FRAME_RATE pkt\n");
				return;
			}
			frame_duration = 1000000 / (*((uint32_t *)p->data));
			break;

		case PKT_TYPE_SET_DURATION:
			if (p->pkt_len != 12)
			{
				LOG("PKTS: WARNING: dropping short SET_DURATION pkt\n");
				return;
			}
			frame_duration = *((uint32_t *)p->data);
			break;

		/* out-of-band immediate commands follow */
		/* but were handled already async when entering gigargoyle, see in_packet() */
		case PKT_TYPE_FLUSH_FIFO:
		case PKT_TYPE_SHUTDOWN:
		default:
			return; /* drop unsupported packages */
	}

	if (p->hdr & PKT_MASK_DBL_BUF)
		flip_double_buffer();

	serve_web_clients();
}

void serve_web_clients(void)
{
	int ret;
	int i;
	for (i=0; i<MAX_WEB_CLIENTS; i++)
	{
		if (web[i] != -1)
		{
			//LOG("PKTS: serving wizard%d\n", i);
			ret = write(web[i], shadow_screen, ACAB_X*ACAB_Y*3);
			if (ret != ACAB_X*ACAB_Y*3)
			{
				LOG("PKTS: gigargoyle told wizard%d to move aside. she refused and is now dead.\n", i);
				close(web[i]);
				web[i] = -1;
			}
		}
	}
}
