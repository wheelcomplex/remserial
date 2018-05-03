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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <syslog.h>
#include <signal.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdarg.h>

#ifndef REMSERIAL_H
#define REMSERIAL_H

void sighandler(int sig);
void link_slave(int fd);
int connect_to(struct sockaddr_in *addr);
void usage(char *progname);
void applog(int priority, const char *format, ...);

#endif
