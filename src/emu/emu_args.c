/* Copyright (c) 2021-2023 Barcelona Supercomputing Center (BSC)
 * SPDX-License-Identifier: GPL-3.0-or-later */

#define _POSIX_C_SOURCE 2

#include "emu_args.h"

#include "common.h"
#include <unistd.h>
#include <string.h>

static char progname[] = "ovniemu";

static void
usage(void)
{
	rerr("Usage: %s [-c offsetfile] [-lh] tracedir\n", progname);
	rerr("\n");
	rerr("Options:\n");
	rerr("  -c offsetfile      Use the given offset file to correct\n");
	rerr("                     the clocks among nodes. It can be\n");
	rerr("                     generated by the ovnisync program\n");
	rerr("\n");
	rerr("  -l                 Enable linter mode. Extra tests will\n");
	rerr("                     be performed.\n");
	rerr("\n");
	rerr("  -h                 Show help.\n");
	rerr("\n");
	rerr("  tracedir           The output trace dir generated by ovni.\n");
	rerr("\n");
	rerr("The output PRV files are placed in the tracedir directory.\n");

	exit(EXIT_FAILURE);
}

void
emu_args_init(struct emu_args *args, int argc, char *argv[])
{
	memset(args, 0, sizeof(struct emu_args));

	int opt;
	while ((opt = getopt(argc, argv, "c:lh")) != -1) {
		switch (opt) {
			case 'c':
				args->clock_offset_file = optarg;
				break;
			case 'l':
				args->linter_mode = 1;
				break;
			case 'h':
			default: /* '?' */
				usage();
		}
	}

	if (optind >= argc) {
		err("missing tracedir\n");
		usage();
	}

	args->tracedir = argv[optind];
}
