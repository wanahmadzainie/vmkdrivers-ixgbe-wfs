/******************************************************************************
 * wfsctl.c     WFS Module. access /dev/wfsring device
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

/*
 * device specifics, such as ioctl numbers and the
 * major device file.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/errno.h>
#include <getopt.h>
#include <fcntl.h>      /* open */
#include <unistd.h>     /* exit */
#include <sys/ioctl.h>      /* ioctl */
#include <ixgbe_wfsctl.h>
#include "wfsctl_cmd.h"

#define PACKAGE_NAME     "wfsctl-utils"
#define PACKAGE_VERSION  "1.0.0"

static char *cmdargv[16];
static int cmdargc;

char *myDevName = NULL;

/*
 * Functions for the ioctl calls
 */

static int dev_open(const char *dev)
{
    char devName[32];
    int fd, retval;
    int i;

    sprintf(devName, "/dev/%s", dev);
    fd = open((const char *)devName, 0);
    if (fd < 0) {
        return(-1);
    }

    return fd;
}

static int dev_close(int fd)
{
    close(fd);
    return 0;
}

static void usage(void)
{
    top_menu[0].handler(0,0,0);
}

/*
 * Main - Call the ioctl functions
 */

int main(int argc, char ** argv)
{
    const struct command *prev_cmd, *cmd, *menu;
    int f, i, devfd, retval, nargs;
    int show_help_only = 0;
    char cmdline[256];
    static const struct option options[] = {
        { .name = "help", .val = 'h' },
        { .name = "version", .val = 'v' },
        { 0 }
    };

    /* parse options */
    while ((f = getopt_long(argc, argv, "vh", options, NULL)) != EOF) {
        switch(f) {
        case 'h':
            show_help_only = 1;
            break;
        case 'v':
            printf("%s v%s\n", PACKAGE_NAME, PACKAGE_VERSION);
            return 0;
        default:
            fprintf(stderr, "Unknown option '%c'\n", f);
            goto print_help;
        }
    }

    if (argc <= optind)
        goto print_help;

    argc -= optind;
    argv += optind;

    /* parse device */
    myDevName = *argv;
    argc--; argv++;

    if (!argc) {
        if (!show_help_only)
            fprintf(stderr, "ERROR: Missing commands.\n");
        goto print_help;
    }

    if (!show_help_only && (devfd = dev_open(myDevName)) < 0) {
        fprintf(stderr, "Can't open device '/dev/%s': %s\n", myDevName, strerror(errno));
        goto err_out;
    }

    /* parse command */
    menu = top_menu;
    prev_cmd = cmd = NULL;
    sprintf(cmdline, "wfsctl %s", myDevName);
    cmdargc = nargs = 0;
    while (argc && menu) {
        sprintf(cmdline, "%s %s", cmdline, *argv);
        if ((cmd = command_parse(menu, *argv)) == NULL) {
            fprintf(stderr, "%s\n", cmdline);
            fprintf(stderr, "%*s\n", strlen(cmdline), "^");
            fprintf(stderr, "ERROR: Invalid command [%s].\n", *argv);
            goto err_out;
        }
        argc--, argv++; nargs = cmd->nargs;
        if (cmd->type == MENUCMD) {
            for (; nargs>0 && argc>0; nargs--) {
                sprintf(cmdline, "%s %s", cmdline, *argv);
                cmdargv[cmdargc] = *argv;
                argc--; argv++; cmdargc++;
            }
        }
        /* get submenu */
        menu = cmd->submenu;
    }

    if (argc) {
        fprintf(stderr, "%s %s\n", cmdline, *argv);
        fprintf(stderr, "%*s\n", strlen(cmdline) + 2, "^");
        fprintf(stderr, "ERROR: Extra token.\n");
        goto err_out;
    }

    if (cmd->type == SUBMENU) {
        fprintf(stderr, "ERROR: Incomplete command.\n");
        fprintf(stderr, "Usage: %s [%s-command]\n", cmdline, cmd->name);
        print_menu_help(cmd->submenu, "");
        goto print_help;
    }

    if (show_help_only) {
        print_command_help(cmd, cmdline);
        return 0;
    }

    if (cmd->type == MENUCMD && nargs > 0) {
        fprintf(stderr, "ERROR: Incorrect number of arguments for the command.\n");
        fprintf(stderr, "Usage: %s %s\t\t%s\n", cmdline, cmd->arglist, cmd->help);
        goto err_out;
    }

#ifdef DEBUG
    printf("command [%s] with %d arguments\n", cmd->name, cmdargc);
    for (i=0; i<cmdargc; i++)
        printf("arg[%d]=%s\n", i, cmdargv[i]);
#endif

#if 0
    if ((devfd = dev_open(myDevName)) < 0) {
        fprintf(stderr, "Can't open device: %s\n", strerror(errno));
        return 1;
    }
#endif

    retval = cmd->handler(devfd, cmdargc, cmdargv);

    dev_close(devfd);

    return retval;

print_help:
    usage();
err_out:
    if (devfd >= 0)
        dev_close(devfd);
    return 1;
}
