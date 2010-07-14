#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include "packets.h"

#define ACAB_IP    (  127  \
                  |(    0  \
             << 8)|(    0  \
             <<16)|(    1  \
             <<24))

#define ACAB_PORT 0Xacab

int s; /* socket to gigargoyle */

void send_pkt(pkt_t * p){
	int ret;
	ret = write(s, p, p->pkt_len);
	if (ret != p->pkt_len)
	{
		printf("ERROR: send_pkt(): %s\n", strerror(errno));
		exit(1);
	}
	usleep(20*1000); /* to avoid padded packets (for now) FIXME! */
}

void init_socket(void)
{
	int ret;
	struct sockaddr_in sa;
	s = socket(AF_INET, SOCK_STREAM, 0);
	if (s < 0)
	{
		printf("ERROR: connect(): %s\n", strerror(errno));
		exit(1);
	}
	memset(&sa, 0, sizeof(sa));
	sa.sin_family        = AF_INET;
	sa.sin_port          = ACAB_PORT;
	sa.sin_addr.s_addr   = ACAB_IP;
	ret = connect(s, (struct sockaddr *) &sa, sizeof(sa));
	if (ret < 0)
	{
		printf("ERROR: connect(): %s\n", strerror(errno));
		exit(1);
	}
}

int main(int argc, char ** argv)
{
	init_socket();

	pkt_t p;
	p.hdr  = VERSION << VERSION_SHIFT; /* header               */
	p.hdr |= PKT_MASK_DBL_BUF;         /* as we use it for all */
	p.hdr |= PKT_MASK_RGB8;            /* following packets    */

	p.pkt_len = 8; /* header only, no payload,
	                  we need to adapt this later for payloaded pkts */

	p.hdr |= PKT_TYPE_SET_SCREEN_BLK; /* command: black screen */
	send_pkt(&p);
	p.hdr &= ~PKT_MASK_TYPE;          /* clear command */
	p.hdr |= PKT_TYPE_FLIP_DBL_BUF;   /* command: flip double buffer */
	send_pkt(&p);

	usleep(1000 * 1000);

	p.hdr &= ~PKT_MASK_TYPE;          /* clear command */
	p.hdr |= PKT_TYPE_SET_SCREEN_WHT; /* command: white screen */
	send_pkt(&p);
	p.hdr &= ~PKT_MASK_TYPE;          /* clear command */
	p.hdr |= PKT_TYPE_FLIP_DBL_BUF;   /* command: flip double buffer */
	send_pkt(&p);
	usleep(1000 * 1000);

	int i;

	p.hdr &= ~PKT_MASK_TYPE;          /* clear command */
	p.hdr |= PKT_TYPE_SET_SCREEN_RND_BW; /* command: random bw screen */
	for (i=0; i<19; i++)
	{
		send_pkt(&p);
		usleep(100 * 1000);
	}

	p.hdr &= ~PKT_MASK_TYPE;          /* clear command */
	p.hdr |= PKT_TYPE_SET_SCREEN_RND_COL; /* command: random color screen */
	for (i=0; i<19; i++)
	{
		send_pkt(&p);
		usleep(100 * 1000);
	}

	return 0;
}
