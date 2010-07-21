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

pkt_t * p; /* network packet from is or qm */

int qm_l;   /* file handle for the queing manager listen()   */
int qm;     /* file handle for the queing manager accept()ed */
int qm_state = QM_NOT_CONNECTED;

int is_l;   /* file handle for instant streamer listen()    */
int is;     /* file handle for instant streamer accept()ed  */
int is_state = IS_NOT_CONNECTED;

int web_l;  /* file handle for web clients listen()          */
int web_state = WEB_NOT_CONNECTED;

int daemon_pid;

/* moodlamp control stuff */
uint32_t frame_remaining;
uint64_t frame_last_time = 0;

char * buf; /* general purpose buffer */
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

	ret = accept(web_l, (struct sockaddr *)&ca, &salen);
	if (ret < 0)
	{
		LOG("ERROR: accept() in  process_web_l_data(): %s\n",
				strerror(errno));
		exit(1);
	}
	int i;
	for (i=0; i<MAX_WEB_CLIENTS; i++){
		if (web[i] == -1)
		{
			web[i] = ret;
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
	ret = close(qm);
	if(ret)
	{
		LOG("ERROR: close(qm): %s\n",
				strerror(errno));
	}
	init_qm_l_socket();
	qm_state = QM_NOT_CONNECTED;
	if (source == SOURCE_QM)
		source = SOURCE_LOCAL;
	LOG("MAIN: listening for new QM connections\n");
}

void process_qm_l_data(void)  {

	int ret;
	struct sockaddr_in ca;
	socklen_t salen = sizeof(struct sockaddr);

	ret = accept(qm_l, (struct sockaddr *)&ca, &salen);
	if (ret < 0)
	{
		LOG("ERROR: accept() in  process_qm_l_data(): %s\n",
				strerror(errno));
		exit(1);
	}
	qm = ret;
	qm_state = QM_CONNECTED;
	if (source != SOURCE_IS)
	{
		source = SOURCE_QM;
		flush_fifo();
	}
	LOG("MAIN: queuing manager connected from %d.%d.%d.%d:%d\n",
			(ca.sin_addr.s_addr & 0x000000ff) >>  0,
			(ca.sin_addr.s_addr & 0x0000ff00) >>  8,
			(ca.sin_addr.s_addr & 0x00ff0000) >> 16,
			(ca.sin_addr.s_addr & 0xff000000) >> 24,
			ntohs(ca.sin_port)
	   );
	ret = close(qm_l);
	if (ret)
	{
		LOG("ERROR: close(qm_l): %s\n", strerror(errno));
	}
}

void process_qm_data(void) {
        static int off = 0;
	int ret;
	LOG("read\n");
	ret = read(qm, buf+off, BUF_SZ-off);
	if (ret == 0)
	{
		LOG("QM closed connection\n");
		close_qm();
		return;
	}
	if (ret < 0)
	{
		LOG("ERROR: read() from queing manager: %s\n",
		    strerror(errno));
		exit(1);
	}

	p = (pkt_t *) buf;
	p->hdr = ntohl(p->hdr);
	p->pkt_len = ntohl(p->pkt_len);
	p->data = (uint8_t *) &buf[8];

	int plen = ret+off;
	int ret_pkt;
        do {
                if(off == 0) {
                    p->hdr = ntohl(p->hdr);
                    p->pkt_len = ntohl(p->pkt_len);
                }

		ret_pkt = in_packet(p, plen);

		if(ret_pkt == -1) {
		    LOG("too short\n");
		    off += plen;
		} else {
		    LOG("frameproc\n");
		    if((int)p->pkt_len <= plen) {
			plen -= (int)p->pkt_len;
			memmove(buf, buf + p->pkt_len, plen);
		    }
		    off = 0;
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
	timestamp <<= 32;
	timestamp +=  tv.tv_usec % 1000000;
	return timestamp;
}

void open_logfile(void)
{
	logfd = open(arguments.log_file,
	             O_WRONLY | O_CREAT,
	             S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
	if (logfd < 0)
	{
		printf("ERROR: open(%s): %s\n", arguments.log_file, strerror(errno));
		exit(1);
	}
	logfp = fdopen(logfd, "a");
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

	daemon_pid = getpid();

	LOG("MAIN: gigargoyle starting up as pid %d\n", daemon_pid);
	int pidfile = open(arguments.pid_file, 
	                   O_WRONLY | O_CREAT | O_TRUNC,
	                   S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
	if (pidfile < 0)
		exit(1);
	snprintf(buf, 6, "%d", daemon_pid);
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
	row[0] = open(arguments.row_0_uart, O_RDWR | O_EXCL);
	uerr[0] = errno;
	row[1] = open(arguments.row_1_uart, O_RDWR | O_EXCL);
	uerr[1] = errno;
	row[2] = open(arguments.row_2_uart, O_RDWR | O_EXCL);
	uerr[2] = errno;
	row[3] = open(arguments.row_3_uart, O_RDWR | O_EXCL);
	uerr[3] = errno;

	int do_exit = 0;
	int i;
	for (i=0; i<4; i++)
	{
		if (row[i] < 0)
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

	qm_l = socket (AF_INET, SOCK_STREAM, 0);
	if (qm_l < 0)
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
        if(setsockopt(qm_l, SOL_SOCKET, SO_REUSEADDR,
                      (char *)&ret, sizeof(ret)) < 0)
        {
            LOG("ERROR: setsockopt() for queuing manager: %s\n",
		    strerror(errno));
		exit(1);
        }

	int bind_retries = 4;
	while (bind_retries--)
	{
		ret = bind(qm_l, (struct sockaddr *) &sa, sizeof(sa));
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
		qm_state = QM_ERROR;
		return;
	}

	ret = listen(qm_l, 8);
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

	web_l = socket (AF_INET, SOCK_STREAM, 0);
	if (web_l < 0)
	{
		LOG("ERROR: socket() for web clients: %s\n",
		    strerror(errno));
		exit(1);
	}
	memset(&sa, 0, sizeof(sa));
	sa.sin_family      = AF_INET;
	sa.sin_addr.s_addr = htonl(INADDR_ANY);
	sa.sin_port        = htons(PORT_WEB);

	int bind_retries = 4;
	while (bind_retries--)
	{
		ret = bind(web_l, (struct sockaddr *) &sa, sizeof(sa));
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
		web_state = WEB_ERROR;
		return;
	}

	ret = listen(web_l, 8);
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
	web = malloc(MAX_WEB_CLIENTS * sizeof(*web));
	if (!web)
	{
		LOG("MAIN: out of memory =(\n");
		exit(1);
	}
	memset(web, -1, MAX_WEB_CLIENTS * sizeof(*web));
	memset(shadow_screen, 0, ACAB_X*ACAB_Y*3);
}

void init(void)
{
	buf = malloc(BUF_SZ);
	if (!buf)
	{
		printf("ERROR: couldn't alloc %d buffer bytes: %s\n",
		       BUF_SZ,
		       strerror(errno));
		exit(1);
	}
	p = malloc(sizeof(pkt_t));
	if (!p)
	{
		printf("ERROR: couldn't alloc %d packet bytes: %s\n",
		       BUF_SZ,
		       strerror(errno));
		exit(1);
	}
	
	if (arguments.foreground)
	{
		logfd = 0;
		logfp = stdout;
	}
	else
	{
		open_logfile();
		daemonize();
	}
	pid_t daemon_pid = getpid();
	LOG("gigargoyle starting up as pid %d\n", daemon_pid);

	atexit(cleanup);
	signal(SIGTERM, sighandler);
        init_uarts();
	init_sockets();
	init_fifo();

	source = SOURCE_LOCAL;
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
		tv.tv_sec  = 0;
		tv.tv_usec = frame_remaining;

		FD_ZERO(&rfd);
		FD_ZERO(&wfd);
		FD_ZERO(&efd);

		/* row */
		for (i=0; i<4; i++)
			FD_SET(row[i], &rfd);
		for (i=0; i<4; i++)
			FD_SET(row[i], &efd);
		nfds = -1;
		for (i=0; i<4; i++)
			nfds = max_int(nfds, row[i]);

		/* qm queuing manager, max 1 */
		if (qm_state != QM_ERROR)
		{
			if (qm_state == QM_NOT_CONNECTED)
			{
				FD_SET(qm_l, &rfd);
				FD_SET(qm_l, &efd);
				nfds = max_int(nfds, qm_l);
			}
			if (qm_state == QM_CONNECTED)
			{
				FD_SET(qm, &rfd);
				FD_SET(qm, &efd);
				nfds = max_int(nfds, qm);
			}
		}

		/* is instant streamer client, max 1 */
		if (is_state == IS_NOT_CONNECTED)
		{
			FD_SET(is_l, &rfd);
			FD_SET(is_l, &efd);
			nfds = max_int(nfds, is_l);
		}
		if (is_state == IS_CONNECTED)
		{
			FD_SET(is, &rfd);
			FD_SET(is, &efd);
			nfds = max_int(nfds, is);
		}

		/* web */
		if (web_state != WEB_ERROR)
		{
			FD_SET(web_l, &rfd);
			FD_SET(web_l, &efd);
			nfds = max_int(nfds, web_l);

			for (i=0; i< MAX_WEB_CLIENTS; i++)
			{
				if (web[i]>=0)
				{
					FD_SET(web[i], &rfd);
					FD_SET(web[i], &efd);
					nfds = max_int(nfds, web[i]);
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
			if (FD_ISSET(row[i], &efd))
			{
				LOG("ERROR: select() error on tty %d: %s\n",
					i,
					strerror(errno));
				exit(1);
			}

		/* qm queuing manager, max 1 */
		if (qm_state == QM_NOT_CONNECTED)
		{
			if (FD_ISSET(qm_l, &efd))
			{
				LOG("ERROR: select() on queuing manager listener: %s\n",
						strerror(errno));
				exit(1);
			}
		}
		if (qm_state == QM_CONNECTED)
		{
			if (FD_ISSET(qm, &efd))
			{
				LOG("MAIN: WARNING: select() on queuing manager connection: %s\n",
						strerror(errno));
				close_qm();
			}
		}

		/* is instant streamer client, max 1 */
		if (is_state == IS_NOT_CONNECTED)
		{
			if (FD_ISSET(is_l, &efd))
			{
				LOG("ERROR: select() on instant streamer listener: %s\n",
						strerror(errno));
				exit(1);
			}
		}
		if (is_state == IS_CONNECTED)
		{
			if (FD_ISSET(is, &efd))
			{
				LOG("ERROR: select() on instant streamer connection: %s\n",
						strerror(errno));
				exit(1);
			}
		}

		if (FD_ISSET(web_l, &efd))
		{
			LOG("ERROR: select() on web client: %s\n",
					strerror(errno));
			exit(1);
		}
		for (i=0; i< MAX_WEB_CLIENTS; i++)
		{
			if (web[i]>=0)
			{
				if (FD_ISSET(web[i], &efd))
				{
					close(web[i]); /* disregarding errors */
					web[i] = -1;
					LOG("WEB: wizard%d stepped into his own trap and seeks for help elsewhere now. mana=852, health=100%%\n", i);
				}
			}
		}

		/* data handling */

		/* we shouldn't get any data from the master
		 * (only used in bootstrapping, not our business)
		 * but at least we log it */
		for (i=0; i<4; i++)
			if (FD_ISSET(row[i], &rfd))
				process_row_data(i);

		if (qm_state == QM_NOT_CONNECTED)
		{
			if (FD_ISSET(qm_l, &rfd))
				process_qm_l_data();
		}
		if (qm_state == QM_CONNECTED)
		{
			if (FD_ISSET(qm, &rfd))
				process_qm_data();
		}

		if (is_state == IS_NOT_CONNECTED)
		{
			if (FD_ISSET(is_l, &rfd))
				process_is_l_data();
		}
		if (is_state == IS_CONNECTED)
		{
			if (FD_ISSET(is, &rfd))
				process_is_data();
		}
		if (FD_ISSET(web_l, &rfd))
			process_web_l_data();

		for (i=0; i< MAX_WEB_CLIENTS; i++)
		{
			if (web[i]>=0)
			{
				if (FD_ISSET(web[i], &rfd))
				{
					/* kill anyone who sends data, stfu   */
					close(web[i]); /* disregarding errors */
					web[i] = -1;

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
			frame_remaining = frame_duration;
			next_frame();
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
