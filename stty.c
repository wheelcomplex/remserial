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
 */

#include "stty.h"

extern int errno;

static void set_this_tty(struct termios *term,struct sttyset *p,int turnon)
{
	/*
	pdebug(5,"set_this_tty: setting %s on? %d\n",p->name,turnon);
	*/
	switch ( p->which ) {
	case CFLG:
		term->c_cflag &= ~(p->mask);
		if ( turnon )
			term->c_cflag |= p->value;
		break;
	case IFLG:
		term->c_iflag &= ~(p->mask);
		if ( turnon )
			term->c_iflag |= p->value;
		break;
	case OFLG:
		term->c_oflag &= ~(p->mask);
		if ( turnon )
			term->c_oflag |= p->value;
		break;
	case LFLG:
		term->c_lflag &= ~(p->mask);
		if ( turnon )
			term->c_lflag |= p->value;
		break;
	case RFLG:
		term->c_iflag = 0;
		term->c_oflag = 0;
		term->c_lflag = 0;
		term->c_cc[VMIN] = 1;
		term->c_cc[VTIME] = 0;
		break;
	case BFLG:
		cfsetispeed(term, p->value);
		cfsetospeed(term, p->value);
		break;
	}
}

int
set_tty(int fd,char *settings)
{
	register char *p;
	register char *s;
	struct termios term;
	register int i;
	int mode;

	/*
	pdebug(4,"set_tty: fd %d settings %s\n",fd,settings);
	*/
	if ( tcgetattr(fd,&term) == -1 ) {
		/*
		pdebug(4,"set_tty: cannot get settings for fd %d, error %d\n",
			fd,errno);
		*/
		return -1;
	}

	s = strdup(settings);
	p = strtok(s," \t\n");
	while (p) {
		mode = 1;
		if ( *p == '-' ) {
			mode = 0;
			p++;
		}
		for ( i=0 ; sttynames[i].name ; i++ ) {
			if ( !strcmp(p,sttynames[i].name) ) {
				set_this_tty(&term,&sttynames[i],mode);
				break;
			}
		}
		p = strtok(NULL," \t\n");
	}
	free(s);
	if ( tcsetattr(fd,TCSANOW,&term) == -1 ) {
		/*
		pdebug(4,"set_tty: cannot get settings for fd %d error %d\n",
			fd,errno);
		*/
		return -1;
	}
	else
		return 0;
}
