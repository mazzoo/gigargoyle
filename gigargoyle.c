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
 * as part of the 
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
#include <errno.h>

#include "config.h"
#include "packets.h"

pkt_t p; /* network packet from is or qm */

int row[4]; /* file handles for the uarts */

int qm_l;   /* file handle for the queing manager listen()   */
int qm;     /* file handle for the queing manager accept()ed */
int qm_state = QM_NOT_CONNECTED;

int is_l;   /* file handle for instant streamer listen()    */
int is;     /* file handle for instant streamer accept()ed  */
int is_state = IS_NOT_CONNECTED;

int web_l;  /* file handle for web clients listen()          */
int web[MAX_WEB_CLIENTS];  /* file handle for web clients accept()ed        */

/* logging */
int logfd;    /* logfile descriptor */
FILE * logfp;
#define LOG(fmt, args...) {fprintf(logfp, fmt, ##args); fflush(logfp);}

int daemon_pid;

char * buf; /* general purpose buffer */
#define BUF_SZ 4096

/* prototypes we need */
void init_qm_l_socket(void);

/* processors of input data from various sources */

void process_row_data(int i) {LOG("row_data()\n");}
void process_is_l_data(void) {LOG("is_l_data()\n");}
void process_is_data(void)   {LOG("is_data()\n");}
void process_web_data(void)  {LOG("web_data()\n");}
void process_web_l_data(void){LOG("web_l_data()\n");}

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
	LOG("listening for new QM connections\n");
}

void process_qm_l_data(void)  {

	int ret;
	struct sockaddr_in ca;
	socklen_t salen = sizeof(struct sockaddr);

	ret = accept(qm_l, (struct sockaddr *)&ca, &salen);
	if (ret < 0)
	{
		LOG("ERROR: accept() in  process_qm_data(): %s\n",
				strerror(errno));
		exit(1);
	}
	qm = ret;
	qm_state = QM_CONNECTED;
	LOG("queuing manager connected from %d.%d.%d.%d:%d\n",
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

void process_qm_data(void)  {
	int ret;
	ret = read(qm, buf, BUF_SZ);
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
	LOG("QM: got %d bytes\n", ret);
}

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
	ret = stat(PID_FILE, &sb);
	if ((ret = 0) || (errno != ENOENT))
	{
		printf("ERROR: gigargoyle still running, or stale pid file found.\n");
		printf("       please try restarting after sth like this:\n");
		printf("\n");
		printf("kill `cat %s` ; sleep 1 ; rm -f %s\n", PID_FILE, PID_FILE);
		printf("\n");
		exit(1);
	}

	logfd = open(LOG_FILE,
	             O_WRONLY | O_CREAT,
	             S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
	if (logfd < 0)
	{
		printf("ERROR: open(%s): %s\n", LOG_FILE, strerror(errno));
		exit(1);
	}
	logfp = fdopen(logfd, "a");
	if (!logfp)
	{
		printf("ERROR: fdopen(%s): %s\n", LOG_FILE, strerror(errno));
		exit(1);
	}

	close(0);
	close(1);
	close(2);

	daemon_pid = getpid();

	LOG("gigargoyle starting up as pid %d\n", daemon_pid);

	int pidfile = open(PID_FILE, 
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
	row[0] = open(ROW_0_UART, O_RDWR | O_EXCL);
	uerr[0] = errno;
	row[1] = open(ROW_1_UART, O_RDWR | O_EXCL);
	uerr[1] = errno;
	row[2] = open(ROW_2_UART, O_RDWR | O_EXCL);
	uerr[2] = errno;
	row[3] = open(ROW_3_UART, O_RDWR | O_EXCL);
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
		LOG("removing pidfile %s\n", PID_FILE);

	ret = unlink(PID_FILE);
	if (ret)
	{
		if (logfp)
			LOG("ERROR: couldn't remove %s: %s\n",
			    PID_FILE,
			    strerror(errno));
	}

	if (logfp)
		LOG("exiting.\n");
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
	sa.sin_port        = htons(PORT_QM);

	ret = bind(qm_l, (struct sockaddr *) &sa, sizeof(sa));
	if (ret < 0)
	{
		LOG("ERROR: bind() for queuing manager: %s\n",
		    strerror(errno));
		exit(1);
	}

	ret = listen(qm_l, 8);
	if (ret < 0)
	{
		LOG("ERROR: listen() for queuing manager: %s\n",
		    strerror(errno));
		exit(1);
	}
}

void init_sockets(void)
{
	init_qm_l_socket();
}

void init(void)
{
	buf = malloc(BUF_SZ);
	if (!buf)
	{
		printf("ERROR: couldn't alloc %d bytes: %s\n",
		       BUF_SZ,
		       strerror(errno));
		exit(1);
	}
	daemonize();
	atexit(cleanup);
	signal(SIGTERM, sighandler);
	init_uarts();
	init_sockets();
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

	while(0Xacab)
	{
		/* prepare for select */
		tv.tv_sec  = 1;
		tv.tv_usec = 0;

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
		FD_SET(web_l, &rfd);
		FD_SET(web_l, &efd);
		nfds = max_int(nfds, web_l);


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
				LOG("WARNING: select() on queuing manager connection: %s\n",
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
	}
}

int main(int argc, char ** argv)
{
	init();
	mainloop();
	return 42;
}
