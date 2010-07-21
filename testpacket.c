#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>

#include "packets.h"

#define ACAB_IP    (  127  \
                  |(    0  \
             << 8)|(    0  \
             <<16)|(    1  \
             <<24))

#define ACAB_PORT 0Xacab

int s; /* socket to gigargoyle */

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

void send_pkt(pkt_t * p){
	int ret;

        uint8_t *packet = serialize_packet(p);
 
	ret = write(s, packet, p->pkt_len);
	if (ret != p->pkt_len)
	{
		printf("ERROR: send_pkt(): %s\n", strerror(errno));
		exit(1);
	}
	usleep(10*1000); /* to avoid padded packets (for now) FIXME! */
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
	ret = 1;
	setsockopt( s, SOL_TCP, TCP_NODELAY, &ret, sizeof(ret));
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

//	usleep(1000 * 1000);

	p.hdr &= ~PKT_MASK_TYPE;          /* clear command */
	p.hdr |= PKT_TYPE_SET_SCREEN_WHT; /* command: white screen */
	send_pkt(&p);
	p.hdr &= ~PKT_MASK_TYPE;          /* clear command */
	p.hdr |= PKT_TYPE_FLIP_DBL_BUF;   /* command: flip double buffer */
	send_pkt(&p);
//	usleep(1000 * 1000);

	int i;

	p.hdr &= ~PKT_MASK_TYPE;          /* clear command */
	p.hdr |= PKT_TYPE_SET_SCREEN_RND_COL; /* command: random color screen */
	for (i=0; i<19; i++)
	{
		send_pkt(&p);
		usleep(10 * 1000);
	}

	p.hdr &= ~PKT_MASK_TYPE;          /* clear command */
	p.hdr |= PKT_TYPE_SET_SCREEN_RND_BW; /* command: random bw screen */
	for (i=0; i<19; i++)
	{
		send_pkt(&p);
		usleep(10 * 1000);
	}

	p.hdr &= ~PKT_MASK_TYPE;          /* clear command */
	p.hdr |= PKT_TYPE_SET_SCREEN_RND_COL; /* command: random color screen */
	for (i=0; i<19; i++)
	{
		send_pkt(&p);
		usleep(10 * 1000);
	}

	return 0;
}
