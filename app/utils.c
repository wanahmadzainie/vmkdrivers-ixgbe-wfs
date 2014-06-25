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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <errno.h>
#include <asm/param.h>
#include <signal.h>
#include <unistd.h>
#include <limits.h>

#include <ixgbe_wfsctl.h>
#include "utils.h"

volatile int CTRL_C = 0;

char *mac2str(unsigned char *mac)
{
    static char mac_s[32];
    sprintf(mac_s, "%02x:%02x:%02x:%02x:%02x:%02x",
            mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    return mac_s;
}

char *ip2str(unsigned int ip)
{
    static char ip_s[32];
    sprintf(ip_s, "%d.%d.%d.%d",
            (ip>>24)&0xff, (ip>>16)&0xff, (ip>>8)&0xff, ip&0xff);
    return ip_s;
}

char *port2str(int port)
{
    static char port_s[4][32];
    static i = 0;

    i = ++i % 4;
    if (port < 0)
        sprintf(port_s[i], "-");
    else
        sprintf(port_s[i], "p%d", port);
    return port_s[i];
}

char *num2kmg(float num)
{
    static char num_s[2][32];
    char unit = ' ';
    static i = 0;

    i = ++i % 2;
    if (num >= 1000) {
        unit = 'K';
        num /= 1000;
        if (num >= 1000) {
            unit = 'M';
            num /= 1000;
            if (num >= 1000) {
                unit = 'G';
                num /= 1000;
            }
        }
    }
    sprintf(num_s[i], "%.2f %c", num, unit);
    return num_s[i];
}

char *sec2str(unsigned int sec)
{
    static char time_s[64];
    int dd,hh,mm,ss;

    ss = sec % 60;
    mm = (sec/60) % 60;
    hh = (sec/3600) % 24;
    dd = sec/86400;
    sprintf(time_s, "%dd %dh %dm %ds", dd, hh, mm, ss);

    return time_s;
}

long getnum(char *str, int base)
{
    char *endptr;
    long val;

    errno = 0;    /* To distinguish success/failure after call */
    val = strtol(str, &endptr, base);

    /* Check for various possible errors */
    if ((errno == ERANGE && (val == LONG_MAX || val == LONG_MIN))
            || (errno != 0 && val == 0))
    {
#ifdef DEBUG
        perror("strtol");
#endif
        return(-1);
    }
    return val;
}

long getbw(char *str)
{
    char *endptr;
    long val;
    int unit;

    errno = 0;    /* To distinguish success/failure after call */
    val = strtol(str, &endptr, 10);

    /* Check for various possible errors */
    if ((errno == ERANGE && (val == LONG_MAX || val == LONG_MIN))
            || (errno != 0 && val == 0))
    {
#ifdef DEBUG
        perror("strtol");
#endif
        return(-1);
    }

    unit = 1;
    if (*endptr == 'k' || *endptr =='K')
        unit = 1000;
    else if (*endptr == 'm' || *endptr =='M')
        unit = 1000000;
    else if (*endptr == 'g' || *endptr =='G')
        unit = 1000000000;
    val = val * unit / 8;

    return val;
}

char *pattern2str(int pattern)
{
    static char pattern_s[5][16] = {
            "(all 0's)",
            "(all 1's)",
            "(01)",
            "(10)",
            "(random)" };

    return pattern_s[pattern-1];
}

void INThandler(int sig)
{
     signal(sig, SIG_IGN);
     CTRL_C = 1;
}



