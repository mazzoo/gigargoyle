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
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/time.h>
#include <errno.h>

#include "config.h"
#include "packets.h"
#include "fifo.h"
#include "gigargoyle.h"
#include "command_line_arguments.h"


/* moodlamp control stuff */
uint32_t frame_remaining;
uint64_t frame_last_time = 0;

#define BUF_SZ 4096

/* prototypes we need */
void init_qm_l_socket(void);

/* processors of input data from various sources */

void process_row_data(int i) {/* LOG("row_data()\n"); */}
void process_is_l_data(void) {LOG("is_l_data()\n");}
void process_is_data(void)   {LOG("is_data()\n");}

void process_web_l_data(void)
{
	int ret;
	struct sockaddr_in ca;
	socklen_t salen = sizeof(struct sockaddr);

	ret = accept(ggg->web->listener, (struct sockaddr *)&ca, &salen);
	if (ret < 0)
	{
		LOG("ERROR: accept() in  process_web_l_data(): %s\n",
				strerror(errno));
		exit(1);
	}
	int i;
	for (i=0; i<MAX_WEB_CLIENTS; i++){
		if (ggg->web->sock[i] == -1)
		{
			ggg->web->sock[i] = ret;
			LOG("WEB: wizard%d materialized in front of gigargoyle from %d.%d.%d.%d:%d\n",
					i,
					(ca.sin_addr.s_addr & 0x000000ff) >>  0,
					(ca.sin_addr.s_addr & 0x0000ff00) >>  8,
					(ca.sin_addr.s_addr & 0x00ff0000) >> 16,
					(ca.sin_addr.s_addr & 0xff000000) >> 24,
					ntohs(ca.sin_port)
			   );
			return;
		}
	}
	LOG("WEB: WARNING: no more clients possible! had to reject %d.%d.%d.%d:%d\n",
		(ca.sin_addr.s_addr & 0x000000ff) >>  0,
		(ca.sin_addr.s_addr & 0x0000ff00) >>  8,
		(ca.sin_addr.s_addr & 0x00ff0000) >> 16,
		(ca.sin_addr.s_addr & 0xff000000) >> 24,
		ntohs(ca.sin_port)
	   );
}

/* Contains parsed command line arguments */
extern struct arguments arguments;

char doc[] = "Control a moodlamp matrix using a TCP socket";
char args_doc[] = "";

/* Accepted option */
struct argp_option options[] = {
	{"pretend", 'p', NULL, 0, "Only pretend to send data to ttys but instead just log sent data"},
	{"foreground", 'f', NULL, 0, "Stay in foreground; don't daemonize"},
	{"port-qm", 'q', "PORT_QM", 0, "Listening port for the acabspool"},
	{"port-is", 'i', "PORT_IS", 0, "Listening port for instant streaming clients"},
	{"port-web", 'w', "PORT_WEB", 0, "Listening port for web clients"},
	{"acab-x", 'x', "WIDTH", 0, "Width of matrix in pixels"},
	{"acab-y", 'y', "HEIGHT", 0, "Height of matrix in pixels"},
	{"uart-0", 127+1, "UART_0", 0, "Path to uart-0"},
	{"uart-1", 127+2, "UART_1", 0, "Path to uart-1"},
	{"uart-2", 127+3, "UART_2", 0, "Path to uart-2"},
	{"uart-3", 127+4, "UART_3", 0, "Path to uart-3"},
	{"pidfile", 127+5, "PIDFILE", 0, "Path to pid file"},
	{"logfile", 'l', "LOGFILE", 0, "Path to log file"},
	{0}
};

/* Argument parser */
struct argp argp = {options, parse_opt, args_doc, doc};

void close_qm(void)
{
	int ret;
	ret = close(ggg->qm->sock);
	if(ret)
	{
		LOG("ERROR: close(qm): %s\n",
				strerror(errno));
	}
	init_qm_l_socket();
	ggg->qm->state = QM_NOT_CONNECTED;
	if (ggg->source == SOURCE_QM)
	{
		ggg->source = SOURCE_LOCAL;
		ggg->ss = NULL;
	}
	LOG("MAIN: listening for new QM connections\n");
	ggg->qm->input_offset = 0;
}

void process_qm_l_data(void)
{

	int ret;
	struct sockaddr_in ca;
	socklen_t salen = sizeof(struct sockaddr);

	ret = accept(ggg->qm->listener, (struct sockaddr *)&ca, &salen);
	if (ret < 0)
	{
		LOG("ERROR: accept() in  process_qm_l_data(): %s\n",
				strerror(errno));
		exit(1);
	}
	ggg->qm->sock = ret;
	ggg->qm->state = QM_CONNECTED;
	if (ggg->source != SOURCE_IS)
	{
		ggg->source = SOURCE_QM;
		ggg->ss = ggg->qm;
		flush_fifo();
	}
	LOG("MAIN: queuing manager connected from %d.%d.%d.%d:%d\n",
			(ca.sin_addr.s_addr & 0x000000ff) >>  0,
			(ca.sin_addr.s_addr & 0x0000ff00) >>  8,
			(ca.sin_addr.s_addr & 0x00ff0000) >> 16,
			(ca.sin_addr.s_addr & 0xff000000) >> 24,
			ntohs(ca.sin_port)
	   );
	ret = close(ggg->qm->listener);
	if (ret)
	{
		LOG("ERROR: close(qm->listener): %s\n", strerror(errno));
	}
}

void process_qm_data(void)
{
	pkt_t p;
	pkt_t *pt;
	int ret;

	ret = read(ggg->qm->sock, ggg->qm->buf + ggg->qm->input_offset, BUF_SZ - ggg->qm->input_offset);
	if (ret == 0)
	{
		LOG("QM closed connection\n");
		close_qm();
		return;
	}
	if (ret < 0)
	{
		LOG("WARNING: read() from queing manager: %s\n",
		    strerror(errno));
		close_qm();
		return;
	}

	pt = (pkt_t *) ggg->qm->buf;

	int plen = ret + ggg->qm->input_offset;
	if ((plen > BUF_SZ) || (plen <= 0))
	{ /* reset input buffer */
		LOG("QM: input reset %d\n", plen);
		ggg->qm->input_offset = 0;
		return;
	}
	int ret_pkt;
        do {
		p.hdr = ntohl(pt->hdr);
		p.pkt_len = ntohl(pt->pkt_len);
		p.data = (uint8_t *) &(pt->data);

		if (p.pkt_len < 8)
		{
			LOG("QM: short packet tells me its %d bytes long\n", p.pkt_len);
			return;
		}

		ret_pkt = in_packet(&p, plen);

		if(ret_pkt == -1) {
			ggg->qm->input_offset += plen;
		} else {
			if( ((int)p.pkt_len <= plen) &&
			    ((int)p.pkt_len > 0)     &&
			    ((int)p.pkt_len < FIFO_WIDTH)) {

				plen -= (int)p.pkt_len;

				if ((plen > BUF_SZ) || (plen <= 0))
				{ /* reset input buffer */
					LOG("QM: input reset %d\n", plen);
					ggg->qm->input_offset = 0;
					return;
				}

				memmove(ggg->qm->buf, ggg->qm->buf + p.pkt_len, plen);
			}
			ggg->qm->input_offset = 0;
		}
	} while(ret_pkt == 0 && plen != 0);
}

uint64_t gettimeofday64(void)
{
	uint64_t timestamp;
	struct timeval tv;
	gettimeofday(&tv, NULL);
	timestamp  =  tv.tv_sec;
	timestamp +=  tv.tv_usec / 1000000;
	timestamp *= 1000000;
	timestamp +=  tv.tv_usec % 1000000;
	return timestamp;
}

void open_logfile(void)
{
	ggg->logfd = open(arguments.log_file,
	                  O_WRONLY | O_CREAT,
	                  S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
	if (ggg->logfd < 0)
	{
		printf("ERROR: open(%s): %s\n", arguments.log_file, strerror(errno));
		exit(1);
	}
	logfp = fdopen(ggg->logfd, "a");
	if (!logfp)
	{
		printf("ERROR: fdopen(%s): %s\n", arguments.log_file, strerror(errno));
		exit(1);
	}
}

/* initialisation */

void daemonize(void)
{
	int ret;

	switch (fork()) {
		case 0:
			break;
		case -1:
			printf("ERROR: fork()\n");
			exit(1);
		default:
			exit(0);
	}
	if (setsid() < 0)
	{
		printf("ERROR: setsid()}\n");
		exit(1);
	}
	if (chdir("/") < 0)
	{
		printf("ERROR: chdir()}\n");
		exit(1);
	}

	struct stat sb;
	ret = stat(arguments.pid_file, &sb);
	if ((ret = 0) || (errno != ENOENT))
	{
		printf("ERROR: gigargoyle still running, or stale pid file found.\n");
		printf("       please try restarting after sth like this:\n");
		printf("\n");
		printf("kill `cat %s` ; sleep 1 ; rm -f %s\n", arguments.pid_file, arguments.pid_file);
		printf("\n");
		exit(1);
	}

	close(0);
	close(1);
	close(2);

	ggg->daemon_pid = getpid();

	LOG("MAIN: gigargoyle starting up as pid %d\n", ggg->daemon_pid);
	int pidfile = open(arguments.pid_file, 
	                   O_WRONLY | O_CREAT | O_TRUNC,
	                   S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
	if (pidfile < 0)
		exit(1);
	char buf[BUF_SZ];
	snprintf(buf, 6, "%d", ggg->daemon_pid);
	ret = write(pidfile, buf, strlen(buf));
	if (ret != strlen(buf))
	{
		LOG("ERROR: could not write() pidfile\n");
		exit(1);
	}
	close(pidfile);
}

void init_uarts(void)
{
	int uerr[4];
	ggg->uart[0] = open(arguments.row_0_uart, O_RDWR | O_EXCL);
	uerr[0] = errno;
	ggg->uart[1] = open(arguments.row_1_uart, O_RDWR | O_EXCL);
	uerr[1] = errno;
	ggg->uart[2] = open(arguments.row_2_uart, O_RDWR | O_EXCL);
	uerr[2] = errno;
	ggg->uart[3] = open(arguments.row_3_uart, O_RDWR | O_EXCL);
	uerr[3] = errno;

	int do_exit = 0;
	int i;
	for (i=0; i<4; i++)
	{
		if (ggg->uart[i] < 0)
		{
			LOG("ERROR: open(device %d): %s\n",
			    i,
			    strerror(uerr[i]));
			do_exit = 1;
		}
	}
	if (do_exit)
		exit(1);
}

int cleanup_done = 0;
void cleanup(void)
{
	int ret;

	if (cleanup_done)
		return;

	if (logfp)
		LOG("MAIN: removing pidfile %s\n", PID_FILE);

	ret = unlink(arguments.pid_file);
	if (ret)
	{
		if (logfp)
			LOG("ERROR: couldn't remove %s: %s\n",
			    arguments.pid_file,
			    strerror(errno));
	}

	if (logfp)
		LOG("MAIN: exiting.\n");
	cleanup_done = 1;
}

void sighandler(int s)
{
	LOG("sunrise is near, I am dying on a signal %d\n", s);
	cleanup();
	exit(0);
}

void init_qm_l_socket(void)
{
	int ret;
	struct sockaddr_in sa;

	ggg->qm->input_offset = 0;

	ggg->qm->listener = socket (AF_INET, SOCK_STREAM, 0);
	if (ggg->qm->listener < 0)
	{
		LOG("ERROR: socket() for queuing manager: %s\n",
		    strerror(errno));
		exit(1);
	}
	memset(&sa, 0, sizeof(sa));
	sa.sin_family      = AF_INET;
	sa.sin_addr.s_addr = htonl(INADDR_ANY);
	sa.sin_port        = htons(arguments.port_qm);

	ret = 1;
	if(setsockopt(ggg->qm->listener, SOL_SOCKET, SO_REUSEADDR,
				(char *)&ret, sizeof(ret)) < 0)
	{
		LOG("ERROR: setsockopt() for queuing manager: %s\n",
		    strerror(errno));
		exit(1);
        }

	int bind_retries = 4;
	while (bind_retries--)
	{
		ret = bind(ggg->qm->listener, (struct sockaddr *) &sa, sizeof(sa));
		if (ret < 0)
		{
			LOG("MAIN: WARNING: bind() for queuing manager: %s... retrying %d\n",
					strerror(errno), bind_retries);
			usleep(1000000);
		}else
			break;
	}
	if (ret < 0)
	{
		LOG("MAIN: WARNING: bind() for queuing manager failed. running without. no movie playing possible\n");
		ggg->qm->state = QM_ERROR;
		return;
	}

	ret = listen(ggg->qm->listener, 8);
	if (ret < 0)
	{
		LOG("ERROR: listen() for queuing manager: %s\n",
		    strerror(errno));
		exit(1);
	}
}

void init_web_l_socket(void)
{
	int ret;
	struct sockaddr_in sa;

	ggg->web = malloc(sizeof(*ggg->web));
	if (!ggg->web)
	{
		LOG("ERROR: malloc() for web clients\n");
		exit(1);
	}

	ggg->web->listener = socket (AF_INET, SOCK_STREAM, 0);
	if (ggg->web->listener < 0)
	{
		LOG("ERROR: socket() for web clients: %s\n",
		    strerror(errno));
		exit(1);
	}
	memset(&sa, 0, sizeof(sa));
	sa.sin_family      = AF_INET;
	sa.sin_addr.s_addr = htonl(INADDR_ANY);
	sa.sin_port        = htons(PORT_WEB);

	ret = 1;
	if(setsockopt(ggg->web->listener, SOL_SOCKET, SO_REUSEADDR,
				(char *)&ret, sizeof(ret)) < 0)
	{
		LOG("ERROR: setsockopt() for web: %s\n",
		    strerror(errno));
		exit(1);
        }

	int bind_retries = 4;
	while (bind_retries--)
	{
		ret = bind(ggg->web->listener, (struct sockaddr *) &sa, sizeof(sa));
		if (ret < 0)
		{
			LOG("MAIN: WARNING: bind() for web clients: %s... retrying %d\n",
					strerror(errno), bind_retries);
			usleep(1000000);
		}else
			break;
	}
	if (ret < 0)
	{
		LOG("MAIN: WARNING: bind() for web clients failed. running without. no live streaming possible\n");
		ggg->web->state = WEB_ERROR;
		return;
	}

	ret = listen(ggg->web->listener, 8);
	if (ret < 0)
	{
		LOG("ERROR: listen() for web clients: %s\n",
		    strerror(errno));
		exit(1);
	}
}

void init_sockets(void)
{
	init_qm_l_socket();
	init_web_l_socket();
}

void init_web(void)
{
	ggg->web->sock = malloc(MAX_WEB_CLIENTS * sizeof(*ggg->web->sock));
	if (!ggg->web->sock)
	{
		LOG("MAIN: out of memory =(\n");
		exit(1);
	}
	memset(ggg->web->sock, -1, MAX_WEB_CLIENTS * sizeof(*ggg->web->sock));
	memset(shadow_screen, 0, ACAB_X*ACAB_Y*3);
	ggg->web->state = WEB_NOT_CONNECTED;
}

void init(void)
{
	/* our global structure */
	ggg = malloc(sizeof(*ggg));
	if (!ggg)
	{
		printf("ERROR: couldn't alloc %d bytes: %s\n",
		       sizeof(*ggg),
		       strerror(errno));
		exit(1);
	}
	memset(ggg, 0, sizeof(*ggg));


	/* queuing manager */
	ggg->qm = malloc(sizeof(*ggg->qm));
	if (!ggg->qm)
	{
		printf("ERROR: couldn't alloc %d bytes: %s\n",
		       sizeof(*ggg->qm),
		       strerror(errno));
		exit(1);
	}

	ggg->qm->buf = malloc(BUF_SZ);
	if (!ggg->qm->buf)
	{
		printf("ERROR: couldn't alloc %d buffer bytes: %s\n",
		       BUF_SZ,
		       strerror(errno));
		exit(1);
	}

	ggg->qm->state = QM_NOT_CONNECTED;

	/* instant streaming */
	ggg->is = malloc(sizeof(*ggg->is));
	if (!ggg->is)
	{
		printf("ERROR: couldn't alloc %d bytes: %s\n",
		       sizeof(*ggg->is),
		       strerror(errno));
		exit(1);
	}

	ggg->is->buf = malloc(BUF_SZ);
	if (!ggg->is->buf)
	{
		printf("ERROR: couldn't alloc %d buffer bytes: %s\n",
		       BUF_SZ,
		       strerror(errno));
		exit(1);
	}

	ggg->is->state = IS_NOT_CONNECTED;

	pkt_t *p = malloc(sizeof(pkt_t));
	if (!p)
	{
		printf("ERROR: couldn't alloc %d packet bytes: %s\n",
		       BUF_SZ,
		       strerror(errno));
		exit(1);
	}
	
	if (arguments.foreground)
	{
		ggg->logfd = 0;
		logfp = stdout;
	}
	else
	{
		open_logfile();
		daemonize();
	}
	ggg->daemon_pid = getpid();
	LOG("gigargoyle starting up as pid %d\n", ggg->daemon_pid);

	atexit(cleanup);
	signal(SIGTERM, sighandler);
        init_uarts();
	init_sockets();
	init_fifo();

	ggg->source = SOURCE_LOCAL;
	ggg->ss = NULL;
	frame_duration = STARTUP_FRAME_DURATION;  /* us per frame */

	init_web();
}

int max_int(int a, int b)
{
	if (a>b)
		return a;
	else
		return b;
}

void mainloop(void)
{
	int i, ret;

	fd_set rfd;
	fd_set wfd;
	fd_set efd;

	int nfds;

	struct timeval tv;

	frame_remaining = frame_duration;

	while(0Xacab)
	{
		/* prepare for select */
		tv.tv_sec  = frame_remaining / 1e6;
		tv.tv_usec = frame_remaining % (uint32_t)1e6;

		FD_ZERO(&rfd);
		FD_ZERO(&wfd);
		FD_ZERO(&efd);

		/* row */
		for (i=0; i<4; i++)
			FD_SET(ggg->uart[i], &efd);
		nfds = -1;
		for (i=0; i<4; i++)
			nfds = max_int(nfds, ggg->uart[i]);

		/* qm queuing manager, max 1 */
		if (ggg->qm->state != QM_ERROR)
		{
			if (ggg->qm->state == QM_NOT_CONNECTED)
			{
				FD_SET(ggg->qm->listener, &rfd);
				FD_SET(ggg->qm->listener, &efd);
				nfds = max_int(nfds, ggg->qm->listener);
			}
			if (ggg->qm->state == QM_CONNECTED)
			{
				FD_SET(ggg->qm->sock, &rfd);
				FD_SET(ggg->qm->sock, &efd);
				nfds = max_int(nfds, ggg->qm->sock);
			}
		}

#if 0 /* FIXME: IS */
		/* is instant streamer client, max 1 */
		if (is_state == IS_NOT_CONNECTED)
		{
			FD_SET(ggg->is->listener, &rfd);
			FD_SET(ggg->is->listener, &efd);
			nfds = max_int(nfds, ggg->is->listener);
		}
		if (is_state == IS_CONNECTED)
		{
			FD_SET(ggg->is->sock, &rfd);
			FD_SET(ggg->is->sock, &efd);
			nfds = max_int(nfds, ggg->is->sock);
		}
#endif

		/* web */
		if (ggg->web->state != WEB_ERROR)
		{
			FD_SET(ggg->web->listener, &rfd);
			FD_SET(ggg->web->listener, &efd);
			nfds = max_int(nfds, ggg->web->listener);

			for (i=0; i< MAX_WEB_CLIENTS; i++)
			{
				if (ggg->web->sock[i]>=0)
				{
					FD_SET(ggg->web->sock[i], &rfd);
					FD_SET(ggg->web->sock[i], &efd);
					nfds = max_int(nfds, ggg->web->sock[i]);
				}
			}
		}


		/* select () */

		nfds++;
		ret = select(nfds, &rfd, &wfd, &efd, &tv);


		/* error handling */
		if (ret < 0)
		{
			LOG("ERROR: select(): %s\n",
					strerror(errno));
			exit(1);
		}

		for (i=0; i<4; i++)
			if (FD_ISSET(ggg->uart[i], &efd))
			{
				LOG("ERROR: select() error on tty %d: %s\n",
					i,
					strerror(errno));
				exit(1);
			}

		/* qm queuing manager, max 1 */
		if (ggg->qm->state == QM_NOT_CONNECTED)
		{
			if (FD_ISSET(ggg->qm->listener, &efd))
			{
				LOG("ERROR: select() on queuing manager listener: %s\n",
						strerror(errno));
				exit(1);
			}
		}
		if (ggg->qm->state == QM_CONNECTED)
		{
			if (FD_ISSET(ggg->qm->sock, &efd))
			{
				LOG("MAIN: WARNING: select() on queuing manager connection: %s\n",
						strerror(errno));
				close_qm();
			}
		}

#if 0 /* FIXME: IS */
		/* is instant streamer client, max 1 */
		if (is_state == IS_NOT_CONNECTED)
		{
			if (FD_ISSET(ggg->is->listener, &efd))
			{
				LOG("ERROR: select() on instant streamer listener: %s\n",
						strerror(errno));
				exit(1);
			}
		}
		if (is_state == IS_CONNECTED)
		{
			if (FD_ISSET(ggg->is->sock, &efd))
			{
				LOG("ERROR: select() on instant streamer connection: %s\n",
						strerror(errno));
				exit(1);
			}
		}
#endif

		if (FD_ISSET(ggg->web->listener, &efd))
		{
			LOG("ERROR: select() on web client: %s\n",
					strerror(errno));
			exit(1);
		}
		for (i=0; i< MAX_WEB_CLIENTS; i++)
		{
			if (ggg->web->sock[i]>=0)
			{
				if (FD_ISSET(ggg->web->sock[i], &efd))
				{
					close(ggg->web->sock[i]); /* disregarding errors */
					ggg->web->sock[i] = -1;
					LOG("WEB: wizard%d stepped into his own trap and seeks for help elsewhere now. mana=852, health=100%%\n", i);
				}
			}
		}

		/* data handling */

		/* we shouldn't get any data from the master
		 * (only used in bootstrapping, not our business)
		 * but at least we log it */
		for (i=0; i<4; i++)
			if (FD_ISSET(ggg->uart[i], &rfd))
				process_row_data(i);

		if (ggg->qm->state == QM_NOT_CONNECTED)
		{
			if (FD_ISSET(ggg->qm->listener, &rfd))
				process_qm_l_data();
		}
		if (ggg->qm->state == QM_CONNECTED)
		{
			if (FD_ISSET(ggg->qm->sock, &rfd))
				process_qm_data();
		}

#if 0 /* FIXME: IS */
		if (is_state == IS_NOT_CONNECTED)
		{
			if (FD_ISSET(ggg->is->listener, &rfd))
				process_is_l_data();
		}
		if (is_state == IS_CONNECTED)
		{
			if (FD_ISSET(ggg->is->sock, &rfd))
				process_is_data();
		}
#endif
		if (FD_ISSET(ggg->web->listener, &rfd))
			process_web_l_data();

		for (i=0; i< MAX_WEB_CLIENTS; i++)
		{
			if (ggg->web->sock[i]>=0)
			{
				if (FD_ISSET(ggg->web->sock[i], &rfd))
				{
					/* kill anyone who sends data, stfu   */
					close(ggg->web->sock[i]); /* disregarding errors */
					ggg->web->sock[i] = -1;

					LOG("WEB: ordinary wizard%d tried to hit gigargoyle %ld times. gigargoyle stood still for %ld seconds and won the fight. mana=852, health=100%%\n",
					   i,
					   random()&0xac,
					   (random()&0xb)+1
					   );
				}
			}
		}

		/* if frame_duration is over run next frame */

		uint64_t tmp64 = gettimeofday64();
		if ((frame_duration + frame_last_time <= tmp64 ) ||
		    (frame_last_time == 0))
		{
			frame_last_time = tmp64;
			next_frame();
			frame_remaining = frame_duration;
		}else{
			frame_remaining = frame_last_time + frame_duration - tmp64;
		}
	}
}

void gigargoyle_shutdown(void)
{
	LOG("MAIN: received a shutdown packet - exiting\n");
	exit(0);
}

int main(int argc, char ** argv)
{
	init_arguments(&arguments);
	argp_parse(&argp, argc, argv, 0, 0, &arguments);

	init();

	mainloop();
	return 42;
}
