/*
 * remserial
 * Copyright (C) 2000  Paul Davis, pdavis@lpccomp.bc.ca
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *
 * This program acts as a bridge either between a socket(2) and a
 * serial/parallel port or between a socket and a pseudo-tty.
 */

#include "stty.h"
#include "remserial.h"

struct sockaddr_in addr, remoteaddr;
int sockfd = -1;
int port = 23000;
int debug = 0;
int devfd;
int *remotefd;
char *machinename = NULL;
char *sttyparms = NULL;
char *sdevname = NULL;
char *linkname = NULL;
int isdaemon = 0;
fd_set fdsreaduse;
struct hostent *remotehost;
extern char *ptsname(int fd);
int curConnects = 0;
int maxfd = -1;

int result;
char devbuf[512];
int devbytes;
int remoteaddrlen;
int c;
int waitlogged = 0;
int maxConnects = 1;
int writeonly = 0;

int main(int argc, char *argv[])
{
	extern char *optarg;
	extern int optind;
	register int i;

	openlog("remserial", LOG_PID, LOG_DAEMON);

	while ((c = getopt(argc, argv, "hdl:m:p:r:s:wx:f:")) != EOF)
	{
		switch (c)
		{
		case 'd':
			isdaemon = 1;
			break;
		case 'l':
			linkname = optarg;
			break;
		case 'x':
			debug = atoi(optarg);
			break;
		case 'm':
			maxConnects = atoi(optarg);
			break;
		case 'p':
			port = atoi(optarg);
			break;
		case 'r':
			machinename = optarg;
			break;
		case 's':
			sttyparms = optarg;
			break;
		case 'w':
			writeonly = 1;
			break;
		case 'f':
			sdevname = optarg;
			break;
		case 'h':
		case '?':
			usage(argv[0]);
			exit(1);
		}
	}

	if (sdevname == NULL)
	{
		char *msg = "-f <device>, tty device option not found.";
		applog(LOG_ERR, msg);
		usage(msg);
		exit(1);
	}

	remotefd = (int *)malloc(maxConnects * sizeof(int));

	applog(LOG_NOTICE, "opening %s ...", sdevname);

	// struct group *getgrgid(gid_t gid);

	if (isdaemon == 0)
	{
		applog(LOG_NOTICE, "sdevname=%s, port=%d, stty=%s", sdevname, port, sttyparms);
	}

	if (writeonly)
		devfd = open(sdevname, O_WRONLY);
	else
		devfd = open(sdevname, O_RDWR);
	if (devfd == -1)
	{
		applog(LOG_ERR, "Open of %s failed: %m", sdevname);
		exit(2);
	}

	if (linkname)
		link_slave(devfd);

	if (sttyparms)
	{
		set_tty(devfd, sttyparms);
	}

	signal(SIGINT, sighandler);
	signal(SIGHUP, sighandler);
	signal(SIGTERM, sighandler);

	if (machinename)
	{
		/* We are the client,
		   Find the IP address for the remote machine */
		remotehost = gethostbyname(machinename);
		if (!remotehost)
		{
			applog(LOG_ERR, "Couldn't determine address of %s",
				   machinename);
			exit(3);
		}

		/* Copy it into the addr structure */
		addr.sin_family = AF_INET;
		memcpy(&(addr.sin_addr), remotehost->h_addr_list[0],
			   sizeof(struct in_addr));
		addr.sin_port = htons(port);

		remotefd[curConnects++] = connect_to(&addr);
	}
	else
	{
		/* We are the server */

		/* Open the initial socket for communications */
		sockfd = socket(AF_INET, SOCK_STREAM, 6);
		if (sockfd == -1)
		{
			applog(LOG_ERR, "Can't open socket: %m");
			exit(4);
		}

		int enable = 1;
		if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0)
		{
			applog(LOG_ERR, "setsockopt(SO_REUSEADDR) failed: %m");
		}

		if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEPORT, &enable, sizeof(int)) < 0)
		{
			applog(LOG_ERR, "setsockopt(SO_REUSEPORT) failed: %m");
		}

		addr.sin_family = AF_INET;
		addr.sin_addr.s_addr = 0;
		addr.sin_port = htons(port);

		/* Set up to listen on the given port */
		if (bind(sockfd, (struct sockaddr *)(&addr),
				 sizeof(struct sockaddr_in)) < 0)
		{
			applog(LOG_ERR, "Couldn't bind port %d, aborting: %m", port);
			exit(5);
		}
		applog(LOG_DEBUG, "Bound port");

		/* Tell the system we want to listen on this socket */
		result = listen(sockfd, 4);
		if (result == -1)
		{
			applog(LOG_ERR, "Socket listen failed: %m");
			exit(6);
		}

		applog(LOG_DEBUG, "Done listen");

		applog(LOG_NOTICE, "listening on %d socket %d ...", port, sockfd);
	}

	if (isdaemon)
	{
		setsid();
		close(0);
		close(1);
		close(2);
	}

	while (1)
	{

		int waitms = 3000;
		struct timeval tv;
		tv.tv_sec = 0;
		tv.tv_usec = waitms * 1000;

		/* Wait for data from the listening socket, the device
		   or the remote connection */
		// applog(LOG_DEBUG, "current connections %d/%d, try to select on socket %d", curConnects, maxConnects, sockfd);

		// Select() updates fd_set's, so we need to build fd_set's before each select()call.

		setup_select();

		int active = select(maxfd, &fdsreaduse, NULL, NULL, &tv);
		if (active == 0)
		{
			continue;
		}
		applog(LOG_DEBUG, "current connections %d/%d, select return active %d", curConnects, maxConnects, active);
		if (active == -1)
		{
			break;
		}

		int matched = 0;

		/* Activity on the controlling socket, only on server */
		if (!machinename && FD_ISSET(sockfd, &fdsreaduse))
		{
			// new connection
			int fd;

			matched++;

			/* Accept the remote systems attachment */
			remoteaddrlen = sizeof(struct sockaddr_in);
			fd = accept(sockfd, (struct sockaddr *)(&remoteaddr),
						&remoteaddrlen);

			if (fd == -1)
			{
				applog(LOG_ERR, "accept failed: %m");
			}
			else if (curConnects < maxConnects)
			{
				unsigned long ip;

				remotefd[curConnects++] = fd;
				/* setup_select will watch this new socket */

				ip = ntohl(remoteaddr.sin_addr.s_addr);
				applog(LOG_NOTICE, "Connection from %d.%d.%d.%d",
					   (int)(ip >> 24) & 0xff,
					   (int)(ip >> 16) & 0xff,
					   (int)(ip >> 8) & 0xff,
					   (int)(ip >> 0) & 0xff);
			}
			else
			{
				// Too many connections, just close it to reject
				applog(LOG_DEBUG, "too many connections %d/%d.", curConnects, maxConnects);
				close(fd);
			}
		}
		else
		{
			// old connection or tty device
			applog(LOG_DEBUG, "select ready for old connection or tty device, current connections %d/%d.", curConnects, maxConnects);
		}

		/* Data to read from the device */
		if (FD_ISSET(devfd, &fdsreaduse))
		{
			devbytes = read(devfd, devbuf, 512);
			//if ( debug>1 && devbytes>0 )
			applog(LOG_DEBUG, "Device: %d bytes", devbytes);

			matched++;

			if (devbytes <= 0)
			{
				applog(LOG_DEBUG, "%s closed", sdevname);
				close(devfd);
				FD_CLR(devfd, &fdsreaduse);
				while (1)
				{
					devfd = open(sdevname, O_RDWR);
					if (devfd != -1)
						break;

					applog(LOG_ERR, "Open of %s failed: %m", sdevname);
					if (errno != EIO)
						exit(7);
					sleep(1);
				}

				applog(LOG_DEBUG, "%s re-opened", sdevname);
				if (sttyparms)
					set_tty(devfd, sttyparms);
				if (linkname)
					link_slave(devfd);
			}
			else
				for (i = 0; i < curConnects; i++)
				{
					int out = write(remotefd[i], devbuf, devbytes);
					if (out == -1)
					{
						applog(LOG_ERR, "write out %d bytes from tty to connection#%d failed: %m", devbytes, i);
						closeConn(i);
					}
					else
					{
						applog(LOG_DEBUG, "write out %d bytes from tty to connection#%d", out, i);
					}
				}
		}

		/* Data to read from the remote system */
		for (i = 0; i < curConnects; i++)
		{
			if (FD_ISSET(remotefd[i], &fdsreaduse))
			{

				matched = 1;

				devbytes = read(remotefd[i], devbuf, 512);

				//if ( debug>1 && devbytes>0 )
				applog(LOG_DEBUG, "Remote: %d bytes", devbytes);

				if (devbytes == 0)
				{
					closeConn(i);
				}
				else if (devfd != -1)
				{
					/* Write the data to the device */
					int out = write(devfd, devbuf, devbytes);
					if (out == -1)
					{
						applog(LOG_ERR, "write out %d bytes from connection#%d to %s failed: %m", out, i, sdevname);
					}
					else
					{
						applog(LOG_DEBUG, "write out %d bytes from connection#%d to %s", out, i, sdevname);
					}

					if (debug > 0)
					{
						out = write(remotefd[i], devbuf, devbytes);
						applog(LOG_DEBUG, "echo back %d bytes to connection#%d", out, i);
					}
				}
			}
		}
		if (matched == 0)
		{
			applog(LOG_DEBUG, "select ready but nothing matched");
		}
	}
	close(sockfd);
	for (i = 0; i < curConnects; i++)
		close(remotefd[i]);
}

void setup_select()
{
	maxfd = -1;
	
	FD_ZERO(&fdsreaduse);

	/* Set up the files/sockets for the select() call */
	if (sockfd != -1)
	{
		FD_SET(sockfd, &fdsreaduse);
		if (sockfd >= maxfd)
			maxfd = sockfd + 1;
	}

	if (!writeonly && devfd != -1)
	{
		FD_SET(devfd, &fdsreaduse);
		if (devfd >= maxfd)
			maxfd = devfd + 1;
	}

	for (int i = 0; i < maxConnects; i++)
	{
		FD_SET(remotefd[i], &fdsreaduse);
		if (remotefd[i] >= maxfd)
			maxfd = remotefd[i] + 1;
	}
}

void closeConn(int i)
{
	register int j;

	applog(LOG_NOTICE, "Connection#%d closed", i);
	FD_CLR(remotefd[i], &fdsreaduse);
	close(remotefd[i]);
	curConnects--;
	for (j = i; j < curConnects; j++)
	{
		remotefd[j] = remotefd[j + 1];
	}

	if (machinename)
	{
		/* Wait for the server again */
		remotefd[curConnects++] = connect_to(&addr);
		FD_SET(remotefd[curConnects - 1], &fdsreaduse);
		if (remotefd[curConnects - 1] >= maxfd)
			maxfd = remotefd[curConnects - 1] + 1;
	}
}

void sighandler(int sig)
{
	int i;

	if (sockfd != -1)
		close(sockfd);
	for (i = 0; i < curConnects; i++)
		close(remotefd[i]);
	if (devfd != -1)
		close(devfd);
	if (linkname)
		unlink(linkname);
	applog(LOG_ERR, "Terminating on signal %d", sig);
	exit(0);
}

void link_slave(int fd)
{
	char *slavename;
	int status = grantpt(devfd);
	if (status != -1)
		status = unlockpt(devfd);
	if (status != -1)
	{
		slavename = ptsname(devfd);
		if (slavename)
		{
			// Safety first
			unlink(linkname);
			status = symlink(slavename, linkname);
		}
		else
			status = -1;
	}
	if (status == -1)
	{
		applog(LOG_ERR, "Cannot create link for pseudo-tty: %m");
		exit(8);
	}
}

int connect_to(struct sockaddr_in *addr)
{
	int waitlogged = 0;
	int stat;
	extern int errno;
	int sockfd;

	unsigned long ip = ntohl(addr->sin_addr.s_addr);
	applog(LOG_DEBUG, "Trying to connect to %d.%d.%d.%d",
		   (int)(ip >> 24) & 0xff,
		   (int)(ip >> 16) & 0xff,
		   (int)(ip >> 8) & 0xff,
		   (int)(ip >> 0) & 0xff);

	while (1)
	{
		/* Open the socket for communications */
		sockfd = socket(AF_INET, SOCK_STREAM, 6);
		if (sockfd == -1)
		{
			applog(LOG_ERR, "Can't open socket: %m");
			exit(9);
		}

		/* Try to connect to the remote server,
		   if it fails, keep trying */

		stat = connect(sockfd, (struct sockaddr *)addr,
					   sizeof(struct sockaddr_in));
		if (debug > 1)
			if (stat == -1)
				applog(LOG_NOTICE, "Connect status %d, errno %d: %m\n", stat, errno);
			else
				applog(LOG_NOTICE, "Connect status %d\n", stat);

		if (stat == 0)
			break;
		/* Write a message to applog once */
		if (!waitlogged)
		{
			applog(LOG_NOTICE,
				   "Waiting for server on %s port %d: %m",
				   machinename, port);
			waitlogged = 1;
		}
		close(sockfd);
		sleep(10);
	}
	if (waitlogged || debug > 0)
		applog(LOG_NOTICE,
			   "Connected to server %s port %d",
			   machinename, port);
	return sockfd;
}

void applog(int priority, const char *fmt, ...)
{
	if (priority == LOG_DEBUG && debug < 1)
	{
		return;
	}
	va_list args;
	va_start(args, fmt);
	if (isdaemon == 0)
	{
		printf("%s", "DEBUG: ");
		vprintf(fmt, args);
		printf("%s", "\r\n");
	}
	else
	{
		syslog(priority, fmt, args);
	}
	va_end(args);
}

void usage(char *progname)
{
	printf("Remserial version 1.3.  Usage:\n");
	printf("remserial [-r machinename] [-p netport] [-s \"stty params\"] [-m maxconnect] <-f device>\n\n");

	printf("-r machinename		The remote machine name to connect to.  If not\n");
	printf("			specified, then this is the server side.\n");
	printf("-p netport		Specifiy IP port# (default 23000)\n");
	printf("-s \"stty params\"	If serial port, specify stty parameters, see man stty\n");
	printf("-m max-connections	Maximum number of simultaneous client connections to allow\n");
	printf("-d			Run as a daemon program\n");
	printf("-x debuglevel		Set debug level, 0 is default, 1,2 give more info\n");
	printf("-l linkname		If the device name is a pseudo-tty, create a link to the slave\n");
	printf("-w          		Only write to the device, no reading\n");
	printf("-f device	I/O device, either serial port or pseudo-tty master\n");
	printf("-h          		wshow this help message\n");
}
