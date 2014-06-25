/******************************************************************************
 * wfsctl_cmd.h		WFS Module. access /dev/wfsring device
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

#ifndef _WFSCTL_CMD_H
#define _WFSCTL_CMD_H

enum menu_type {
  SUBMENU,
  MENUCMD,
};

struct command {
    const char *name;
    enum menu_type type;
    const struct command *submenu;
    int (*handler)(int devfd, int argc, char **argv);
    int nargs;
    const char *arglist;
    const char *help;
    const char *long_help;
};

const struct command *command_parse(const struct command menu[], const char *token);
void print_menu_help(const struct command menu[], char *parent_cmd);
void print_command_help(const struct command *cmd, char *parent_cmd);

extern char *myDevName;
extern const struct command top_menu[];
extern const struct command show_menu[];
extern const struct command show_bert_menu[];

#endif
