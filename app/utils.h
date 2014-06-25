/******************************************************************************
 * wfsctl_cmd.c     WFS Module. access /dev/wfsring device
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *****************************************************************************/

#ifndef __WFSUTIL_H__
#define __WFSUTIL_H__

#include <ixgbe_wfsctl.h>

extern volatile int CTRL_C;
extern char *mac2str(unsigned char *mac);
extern char *ip2str(unsigned int ip);
extern char *port2str(int port);
extern long getnum(char *str, int base);
extern long getbw(char *str);
extern char *num2kmg(float num);
extern char *sec2str(unsigned int sec);
extern char *pattern2str(int pattern);
extern void INThandler(int sig);

#endif // __WFSUTIL_H__
