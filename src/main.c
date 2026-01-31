#include "log.h"
#include "wm.h"
#include <stdio.h>
#include <getopt.h>
#include <string.h>

#define SHORTOPTS "hl:qv"

static struct option cmd_opts[] = {
	{"help",    no_argument,       0, 'h'},
	{"listen",  required_argument, 0, 'l'},
	{"quiet",   no_argument,       0, 'q'},
	{"verbose", no_argument,       0, 'v'},
	{NULL}
};

static void print_usage(const char *argv0)
{
	printf("Usage: %s [-h]\n"
	       "\n"
	       "Options:\n"
	       " -h --help             Print this text\n"
	       " -l --listen <path>    Path of the command socket\n"
	       " -q --quiet            Be more quiet\n"
	       " -v --verbose          Be more verbose\n",
	       argv0);
}

int main(int argc, char *argv[])
{
	int opt;
	int err;

	int log_level;

	log_level = LOG_WARN;

	do {
		opt = getopt_long(argc, argv, SHORTOPTS,
		                  cmd_opts, NULL);

		switch (opt) {
		case 'h':
			print_usage(argv[0]);
			return 1;

		case 'v':
			log_level++;
			break;

		case 'q':
			log_level--;
			break;

		default:
			opt = -1;
			break;
		}
	} while (opt >= 0);

	log_set_level(log_level);

	err = wm_init();

	if (!err) {
		err = wm_run();
	}

	return err;
}
