/******************************************************************************
 * wfsctl_cmd.c     WFS Module. access /dev/ringNNN device
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
#include "wfsctl_cmd.h"
#include "utils.h"


static wfsctl_data iocdata;
static wfsctl_peer_data plist[MAX_PEER_LIST];
static wfsctl_fib_data fib[FIB_GET_SIZE];
static wfsctl_bert_cfg bertcfg;
static wfsctl_bert_stats bertstats[2];

#define BERT_STATS_PERIOD   5   // in seconds
#define BERT_STATS_INT      1   // in seconds

#define diff_tx_pkts(s1,s2)     ((s1)->tx_pkts - (s2)->tx_pkts)
#define diff_rx_pkts(s1,s2)     ((s1)->rx_pkts - (s2)->rx_pkts)
#define diff_tx_bps(s1,s2,t)    (((float)((s1)->tx_bytes-(s2)->tx_bytes))*8/(t))
#define diff_rx_bps(s1,s2,t)    (((float)((s1)->rx_bytes-(s2)->rx_bytes))*8/(t))
#define diff_err_csum(s1,s2)     ((s1)->err_csum-(s2)->err_csum)
#define diff_err_drop(s1,s2)    ((s1)->err_drop-(s2)->err_drop)
#define diff_err_size(s1,s2)    ((s1)->err_size-(s2)->err_size)
#define diff_err_seq(s1,s2)     ((s1)->err_seq-(s2)->err_seq)


/*
 * SHOW BERT menu
 */
#define cmd_show_bert_result_help \
        "\t[wfsid]                test data source workstation id\n"
static int cmd_show_bert_result(int fd, int argc, char **argv)
{
    int i, num, fibn, retval;
    wfsctl_bert_stats *stats;
    long val;

    /* parse input */
    if ((val = getnum(argv[0], 10)) < 0 || val < WFSID_MIN || val > WFSID_MAX)  {
        fprintf(stderr, "ERROR: invalid wfsid '%s'.\n", argv[0]);
        return -1;
    }

    /* get final result */
    memset(bertstats, 0, sizeof(bertstats));
    bertstats[0].wfsid = (char)val;
    stats = &bertstats[0];

    iocdata.len = sizeof(wfsctl_bert_stats);
    iocdata.v.bertstats = stats;
    if ((retval = ioctl(fd, WFSCTL_GET_BERT_STATS, &iocdata)) == 0 && stats->interval) {
        ioctl(fd, WFSCTL_GET_BERT_STATS, &iocdata);
        printf("SOURCE   : WFS-ID %d\n", stats->wfsid);
        printf("INTERVAL : %s\n", sec2str(stats->interval));
        printf("REQUEST  : RX %lu pkts, %sB, %sbps\n",
                stats->tx_pkts, num2kmg(stats->tx_bytes), num2kmg(((float)stats->tx_bytes)*8/stats->interval));
        printf("RESPONSE : TX %lu pkts, %sB, %sbps\n",
                stats->rx_pkts, num2kmg(stats->rx_bytes), num2kmg(((float)stats->rx_bytes)*8/stats->interval));
        printf("ERROR    : %lu Dropped(%.3f%%), %lu CRC(%.3f%%), %lu Size(%.3f%%), %lu Sequence(%.3f%%)\n",
                stats->err_drop, ((float)stats->err_drop)/stats->tx_pkts*100,
                stats->err_csum, ((float)stats->err_csum)/stats->tx_pkts*100,
                stats->err_size, ((float)stats->err_size)/stats->tx_pkts*100,
                stats->err_seq, ((float)stats->err_seq)/stats->tx_pkts*100);
    }

    return 0;
}

static int cmd_bert(int fd, int argc, char **argv);
#define cmd_show_bert_interval_help \
        "\t[sec]                  show interval\n"

static int cmd_show_bert(int fd, int argc, char **argv);
const struct command show_bert_menu[] = {
    { "interval", MENUCMD, 0, cmd_show_bert, 1, "[sec]", "show bit error rate test stats", cmd_show_bert_interval_help },
    { "result", MENUCMD, 0, cmd_show_bert_result,
            0, 0, "show previous bit error rate test stats", cmd_show_bert_result_help },
    {0},
};

/*
 * SHOW menu
 */
static int cmd_show_peers(int fd, int argc, char **argv)
{
    int i, num, retval;
    wfsctl_peer_data *peer;

    iocdata.len = sizeof(plist);
    iocdata.v.plist = plist;

    printf("\n  WORKSTATION LIST\n");

    printf("------------------------------------------------------------------------------\n"
           "  WFS_ID  MAC_ADDR           IP_ADDR          PRI/SEC  STATE    TX_PORT       \n"
           "------------------------------------------------------------------------------\n");


    if ((retval = ioctl(fd, WFSCTL_GET_PEER_LIST, &iocdata)) < 0) {
        fprintf(stderr, "ERROR: get workstation list failed, retval = %d\n", retval);
    } else {
        num = iocdata.len / sizeof(wfsctl_peer_data);

        for (i=0; i<num; i++) {
            peer = &iocdata.v.plist[i];
            printf("   %-2c%-3d  %-17s  %-15s  %3s/%-3s  %-7s  %s/%s\n",
                    peer->flag.local ? '*' : ' ',
                    peer->id,
                    mac2str(peer->mac),
                    ip2str(peer->ip),
                    port2str(peer->port_pri), port2str(peer->port_sec),
                    peer->flag.pri ? "IDLE" :  peer->flag.sec ? "PROTECT" : "-",
                    myDevName, port2str(peer->eport));
        }
        printf("------------------------------------------------------------------------------\n");
        printf("*-Local Workstation                      PRI/SEC-Primary/Secondary port number\n");
        printf("IDLE/PROTECT=Peer Rx state               TX_PORT-Local Tx port to peer        \n");

    }

    return 0;
}

static int cmd_show_fib(int fd, int argc, char **argv)
{
    int i, num, fibn, retval;

    iocdata.len = sizeof(fib);
    iocdata.v.fib = fib;

    printf("\n  FORWARDING TABLE\n");
    printf("------------------------------------------------\n"
           "  MAC_ADDR           WFS_ID  INTERFACE          \n"
           "------------------------------------------------\n");
    for (fibn=1; ; ) {
        fib[0].no = fibn; /* set first entry no. to read */

        if ((retval = ioctl(fd, WFSCTL_GET_FIB, &iocdata)) < 0) {
            fprintf(stderr, "ERROR: get MAC forwarding table failed, retval = %d\n", retval);
            break;
        } else {
            num = iocdata.len / sizeof(wfsctl_fib_data);
            if (num == 0)
                break;

            for (i=0; i<num; i++)
                printf("  %-17s  %4d    %s/%s\n", mac2str(fib[i].mac), fib[i].id,
                        myDevName, port2str(fib[i].eport));

            fibn += num;
        }
    }
    printf("------------------------------------------------\n");

    return 0;
}

#define cmd_show_bert_help \
        "\t[wfsid]                test data source workstation id\n"
static int cmd_show_bert(int fd, int argc, char **argv)
{
    int i, num, fibn, retval, wfsid, interval;
    wfsctl_bert_stats *stats2, *stats1;
    long val;
    time_t ts0, ts1, ts2;

    /* parse input */
    if ((val = getnum(argv[0], 10)) < 0 || val < WFSID_MIN || val > WFSID_MAX)  {
        fprintf(stderr, "ERROR: invalid wfsid '%s'.\n", argv[0]);
        return -1;
    }
    wfsid = (unsigned int)val;

    if (argc == 2) {
        if ((val = getnum(argv[1], 10)) <= 0)  {
            fprintf(stderr, "ERROR: invalid interval '%s'.\n", argv[1]);
            return -1;
        }
        interval = (unsigned int)val;
    } else
        interval = 0;

    /* catch CTRL-C for ending test */
    CTRL_C = 0;
    signal(SIGINT, INThandler);

    /* get periodical result */
    memset(bertstats, 0, sizeof(bertstats));
    bertstats[0].wfsid = bertstats[1].wfsid = (char)wfsid;

show_bert_start:

    printf("\n  BIT-ERROR-RATE-TEST STATS : WFS-ID %d\n", val);
    printf("------------------------------------------------------------------------------------------------------\n"
           "  INTERVAL    TX-PKT         TX-BW    RX-PKT         RX-BW  ERR-DROP   ERR-CRC  ERR-SIZE   ERR-SEQ    \n"
           "------------------------------------------------------------------------------------------------------\n");

    /* get first result */
    stats2 = &bertstats[i=0];
    iocdata.len = sizeof(wfsctl_bert_stats);
    iocdata.v.bertstats = stats2;
    ioctl(fd, WFSCTL_GET_BERT_STATS, &iocdata);
    ts0 = ts1 = ts2 = time(0);

    while (!CTRL_C) {
        sleep(BERT_STATS_INT);
        stats1 = &bertstats[i%2];
        stats2 = &bertstats[(++i)%2];
        iocdata.v.bertstats = stats2;

        if ((retval = ioctl(fd, WFSCTL_GET_BERT_STATS, &iocdata)) == 0 && !CTRL_C) {
            ts1 = ts2; ts2 = time(0);
            if (stats1->interval > stats2->interval) {
                /* stats has been reset if previous interval is shorter */
                printf("(STATS RESET)\n");

                /* reset */
                ts0 = ts1 = ts2 = time(0);
                goto show_bert_start;
            }
            if (diff_rx_bps(stats2,stats1,ts2-ts1) == 0)
                continue;
            printf("%4u -%4u  %8ld  %9sbps  %8ld  %9sbps  %8ld  %8ld  %8ld  %8ld\n",
                   ts1-ts0, ts2-ts0,
                   diff_rx_pkts(stats2,stats1), num2kmg(diff_rx_bps(stats2,stats1,ts2-ts1)),
                   diff_tx_pkts(stats2,stats1), num2kmg(diff_tx_bps(stats2,stats1,ts2-ts1)),
                   diff_err_drop(stats2,stats1), diff_err_csum(stats2,stats1),
                   diff_err_size(stats2,stats1), diff_err_seq(stats2,stats1));
        }

        if (interval > 0 && interval <= (ts2-ts0))
            break;
    }
    if (CTRL_C) printf("\n");
    printf("------------------------------------------------------------------------------------------------------\n");

    /* get final result */
    sleep(1);
    iocdata.len = sizeof(wfsctl_bert_stats);
    iocdata.v.bertstats = stats2;
    if ((retval = ioctl(fd, WFSCTL_GET_BERT_STATS, &iocdata)) == 0 && stats2->interval) {
        ioctl(fd, WFSCTL_GET_BERT_STATS, &iocdata);
        printf("SOURCE   : WFS-ID %d\n", stats2->wfsid);
        printf("INTERVAL : %s\n", sec2str(stats2->interval));
        printf("REQUEST  : RX %lu pkts, %sB, %sbps\n",
                stats2->tx_pkts, num2kmg(stats2->tx_bytes), num2kmg(((float)stats2->tx_bytes)*8/stats2->interval));
        printf("RESPONSE : TX %lu pkts, %sB, %sbps\n",
                stats2->rx_pkts, num2kmg(stats2->rx_bytes), num2kmg(((float)stats2->rx_bytes)*8/stats2->interval));
        printf("ERROR    : %lu Dropped(%.3f%%), %lu CRC(%.3f%%), %lu Size(%.3f%%), %lu Sequence(%.3f%%)\n",
                stats2->err_drop, ((float)stats2->err_drop)/stats2->tx_pkts*100,
                stats2->err_csum, ((float)stats2->err_csum)/stats2->tx_pkts*100,
                stats2->err_size, ((float)stats2->err_size)/stats2->tx_pkts*100,
                stats2->err_seq, ((float)stats2->err_seq)/stats2->tx_pkts*100);
    }

    return 0;
}

static int show_menu_help(int fd, int argc, char **argv)
{
    printf("Usage: wfsctl show [peers|fib|bert]\n");
    printf("commands:\n");
    print_menu_help(show_menu, "");
    return 0;
}

const struct command show_menu[] = {
    { "help",  MENUCMD, 0, show_menu_help, 0, 0, "show command help", 0 }, // keep help as 1st element
    { "peers", MENUCMD, 0, cmd_show_peers, 0, 0, "show workstation peers", 0 },
    { "fib",   MENUCMD, 0, cmd_show_fib, 0, 0, "show forwarding table", 0 },
    { "bert",  MENUCMD, show_bert_menu,
            cmd_show_bert, 1, "[wfsid]", "show bit error rate test stats", cmd_show_bert_help },
    {0},
};

/*
 * BERT menu
 */
static int cmd_bert(int fd, int argc, char **argv);
#define cmd_bert_interval_help \
        "\t[sec]                  test interval\n"

const struct command cmd_bert_menu[] = {
    { "interval", MENUCMD, 0, cmd_bert, 1, "[sec]", "perform bit error rate test", cmd_bert_interval_help },
    {0},
};

/*
 * TOP menu
 */
#define cmd_bert_help \
        "\t[wfsid]                test data responder workstation id\n" \
        "\t[pattern]              test data byte pattern, \n" \
        "\t                       bNNNNNNNN for binary, 0NNN for octal, 0xNN for hexadecimal or 'rand' for random number\n" \
        "\t[size]                 test data size in byte, max 9000 bytes \n" \
        "\t[bandwidth][k|m|g]     test bandwidth in [KMG] bits/sec\n"
static int cmd_bert(int fd, int argc, char **argv)
{
    int i, num, fibn, retval, interval, base;
    wfsctl_bert_stats *stats2, *stats1;
    long val;
    time_t ts0, ts1, ts2;
    char *p;

    memset(&bertcfg, 0, sizeof(bertcfg));

    /* parse input */
    if ((val = getnum(argv[0], 10)) < 0 || val < WFSID_MIN || val > WFSID_MAX)  {
        fprintf(stderr, "ERROR: invalid wfsid '%s'.\n", argv[0]);
        return -1;
    }
    bertcfg.wfsid = (u_char)val;

    if (strcmp(argv[1], "rand") == 0) {
        bertcfg.pkt_pattern_flag = 1;
        val = rand() % 256;
    } else {
        if (argv[1][0]=='0' && argv[1][1]=='x') {
            base = 16; p = &argv[1][2];
        } else if (argv[1][0]=='0') {
            base = 8; p = &argv[1][1];
        } else if (argv[1][0]=='b') {
            base = 2; p = &argv[1][1];
        } else {
            base = 10; p = argv[1];
        }
        if ((val = getnum(p, base)) < 0)  {
            fprintf(stderr, "ERROR: invalid pattern '%s'.\n", argv[1]);
            return -1;
        }
    }
    bertcfg.pkt_pattern = (char)val;

    if ((val = getnum(argv[2], 10)) <= 0 || val > BERT_DATA_SIZE_MAX)  {
        fprintf(stderr, "ERROR: invalid data size '%s'.\n", argv[2]);
        return -1;
    }
    bertcfg.pkt_size = (unsigned short)val;

    if ((val = getbw(argv[3])) <= 0)  {
        fprintf(stderr, "ERROR: invalid bandwidth '%s'.\n", argv[3]);
        return -1;
    }
    bertcfg.bandwidth = (unsigned int)val;

    if (argc == 5) {
        if ((val = getnum(argv[4], 10)) <= 0)  {
            fprintf(stderr, "ERROR: invalid interval '%s'.\n", argv[4]);
            return -1;
        }
        interval = (unsigned int)val;
    } else
        interval = 0;

    /* begin test */
    bertcfg.onoff = 1;
    iocdata.len = sizeof(bertcfg);
    iocdata.v.bertcfg = &bertcfg;
    if ((retval = ioctl(fd, WFSCTL_SET_BERT_CFG, &iocdata)) < 0) {
        fprintf(stderr, "ERROR: set BERT configuration failed, retval = %d\n", retval);
        return -1;
    }

    /* catch CTRL-C for ending test */
    CTRL_C = 0;
    signal(SIGINT, INThandler);

    /* get periodical result */
    memset(bertstats, 0, sizeof(bertstats));
    bertstats[0].wfsid = bertstats[1].wfsid = 0; // set 0 to get my own stats

    printf("\n  BIT-ERROR-RATE-TEST STATS\n"
           "------------------------------------------------------------------------------------------------------\n"
           "  INTERVAL    TX-PKT         TX-BW    RX-PKT         RX-BW  ERR-DROP   ERR-CRC  ERR-SIZE   ERR-SEQ    \n"
           "------------------------------------------------------------------------------------------------------\n"
          );


    /* get first result */
    sleep(1);
    stats2 = &bertstats[i=0];
    iocdata.len = sizeof(wfsctl_bert_stats);
    iocdata.v.bertstats = stats2;
    ioctl(fd, WFSCTL_GET_BERT_STATS, &iocdata);
    ts0 = ts1 = ts2 = time(0);

    while (!CTRL_C) {
        sleep(BERT_STATS_INT);
        stats1 = &bertstats[i%2];
        stats2 = &bertstats[(++i)%2];
        iocdata.v.bertstats = stats2;

        if ((retval = ioctl(fd, WFSCTL_GET_BERT_STATS, &iocdata)) == 0 && !CTRL_C) {
            ts1 = ts2; ts2 = time(0);
            printf("%4u -%4u  %8ld  %9sbps  %8ld  %9sbps  %8ld  %8ld  %8ld  %8ld\n",
                    ts1-ts0, ts2-ts0,
                    diff_tx_pkts(stats2,stats1), num2kmg(diff_tx_bps(stats2,stats1,ts2-ts1)),
                    diff_rx_pkts(stats2,stats1), num2kmg(diff_rx_bps(stats2,stats1,ts2-ts1)),
                    diff_err_drop(stats2,stats1), diff_err_csum(stats2,stats1),
                    diff_err_size(stats2,stats1), diff_err_seq(stats2,stats1));
        }

        if (interval > 0 && interval <= stats2->interval)
            break;
    }
    if (CTRL_C) printf("\n");
    printf("------------------------------------------------------------------------------------------------------\n");

    /* end test */
    bertcfg.onoff = 0;
    iocdata.len = sizeof(bertcfg);
    iocdata.v.bertcfg = &bertcfg;
    if ((retval = ioctl(fd, WFSCTL_SET_BERT_CFG, &iocdata)) < 0) {
        fprintf(stderr, "ERROR: set test configration failed, retval = %d\n", retval);
        return -1;
    }

    /* get final result */
    sleep(1);
    iocdata.len = sizeof(wfsctl_bert_stats);
    iocdata.v.bertstats = stats2;
    if ((retval = ioctl(fd, WFSCTL_GET_BERT_STATS, &iocdata)) == 0 && stats2->interval) {
        ioctl(fd, WFSCTL_GET_BERT_STATS, &iocdata);
        printf("TARGET   : WFS-ID %d\n", bertcfg.wfsid);
        if (bertcfg.pkt_pattern_flag)
        printf("DATA     : PKT SIZE %d bytes PATTERN random @ %sbps (%u pkts/s)\n",
                bertcfg.pkt_size, num2kmg((float)bertcfg.bandwidth*8),
                bertcfg.bandwidth/bertcfg.pkt_size);
        else
        printf("DATA     : PKT SIZE %d bytes PATTERN 0x%02X @ %sbps (%u pkts/s)\n",
                bertcfg.pkt_size, bertcfg.pkt_pattern, num2kmg((float)bertcfg.bandwidth*8),
                bertcfg.bandwidth/bertcfg.pkt_size);
        printf("INTERVAL : %s\n", sec2str(stats2->interval));
        printf("REQUEST  : TX %lu pkts, %sB, %sbps\n",
                stats2->tx_pkts, num2kmg(stats2->tx_bytes), num2kmg((float)stats2->tx_bytes*8/stats2->interval));
        printf("RESPONSE : RX %lu pkts, %sB, %sbps\n",
                stats2->rx_pkts, num2kmg(stats2->rx_bytes), num2kmg((float)stats2->rx_bytes*8/stats2->interval));
        printf("RTT      : min/max/avg %.2f/%.2f/%.2f ms\n",
                (float)stats2->rtt_min/1000, (float)stats2->rtt_max/1000, (float)stats2->rtt_avg/1000);
        printf("ERROR    : %lu Dropped(%.3f%%), %lu CRC(%.3f%%), %lu Size(%.3f%%), %lu Sequence(%.3f%%)\n",
                stats2->err_drop, ((float)stats2->err_drop)/stats2->tx_pkts*100,
                stats2->err_csum, ((float)stats2->err_csum)/stats2->tx_pkts*100,
                stats2->err_size, ((float)stats2->err_size)/stats2->tx_pkts*100,
                stats2->err_seq, ((float)stats2->err_seq)/stats2->tx_pkts*100);
    }

    return 0;
}

static int top_menu_help(int fd, int argc, char **argv)
{
    printf("Usage: wfsctl [options] <device> [commands]\n");
    printf("options:\n");
    printf("\t%-45s\t%s\n", "-v", "show version");
    printf("\t%-45s\t%s\n", "-h", "show command help");
    printf("commands:\n");
    print_menu_help(top_menu, "");
    return 0;
}

const struct command top_menu[] = {
    { "help", MENUCMD, 0, top_menu_help, 0, 0, "show this help", 0 }, // keep help as 1st element
	{ "show", SUBMENU, show_menu, NULL, 0, 0, "show command menu", 0 },
    { "bert", MENUCMD, cmd_bert_menu, cmd_bert, 4,
            "[wfsid] [pattern] [size] [bandwidth]", "perform bit error rate test", cmd_bert_help },
	{0},
};


/*
 * menu support functions
 */
const struct command *command_parse(const struct command menu[], const char *token)
{
    int i;

    for (i = 0; menu[i].name; i++) {
        if (!strcmp(token, menu[i].name))
            return &menu[i];
    }

    return NULL;
}

void print_menu_help(const struct command menu[], char *parent_cmd)
{
    char cmdline[256];
	int i;

	for (i = 0; menu[i].name; i++) {
	    sprintf(cmdline, "%s %s", parent_cmd, menu[i].name);
		if (menu[i].type == SUBMENU) {
		    print_menu_help(menu[i].submenu, cmdline);
		} else {
		    if (menu[i].nargs)
		        sprintf(cmdline, "%s %s", cmdline, menu[i].arglist);
		    printf("\t%-45s\t%s\n", cmdline, menu[i].help);
		    if (menu[i].submenu)
		        print_menu_help(menu[i].submenu, cmdline);
		}
	}
}

void print_command_help(const struct command *cmd, char *cmdline)
{
    printf("Usage: %s %s\n", cmdline, (cmd->nargs) ? cmd->arglist : "");
    if (cmd->long_help)
        printf("%s\n", cmd->long_help);
    else
        printf("%s\n", cmd->help);

}


