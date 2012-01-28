/*
 * library_info.c
 *
 * CLI to get static build-time information about this library.
 */

#include <config.h>

#include <stdlib.h>
#include <stdio.h>
#include <syslog.h>
#include <argp.h>

#include <pilot_fish/syslog.h>
#include <pilot_fish/syslog_macros.h>

#include <pilot_fish/bovespa.h>


/*
 * Command line argument parsing.
 */

const char *argp_program_version = PACKAGE_VERSION;
const char *argp_program_bug_address = PACKAGE_BUGREPORT;

static char doc[] = "pfish_bovespa_library_info -- build-time information about pilot_fish bovespa library.\vThis routine shows static build-time information about the pilot_fish bovespa library installed on this system.\n";

static struct argp_option options[] = {

	{ 0 }

};

struct arguments {

};

static error_t parse_opt (int key, char *arg, struct argp_state *state) {

	struct arguments *arguments = state->input;

	switch (key) {

		case ARGP_KEY_ARG:

			argp_usage (state);
			break;

		default:

			return (ARGP_ERR_UNKNOWN);

	};
	return (0);

};

static struct argp argp = { options, parse_opt, NULL, doc };


/*
 * The portal.
 */

#define SUCCESS return (EXIT_SUCCESS)
#define FAILURE return (EXIT_FAILURE)

int main (int argc, char **argv) {

	struct arguments arguments;	// Arguments given in the command line.
	pfish_bovespa_library_info_t library_info;	// Library info structure.

	pfish_syslog_init (SYSLOG_FACILITY, LOG_PERROR);
	DEBUG ("start.");

	/*
	 * Parse command line arguments.
	 */

	argp_parse (&argp, argc, argv, 0, 0, &arguments);

	/*
	 * Export build-time info.
	 */

	pfish_bovespa_library_info_get (&library_info);

#ifdef DEBUGGING
#define DEBUGGING_ANSWER 1
#else
#define DEBUGGING_ANSWER 0
#endif	// DEBUGGING

	printf ("debugging = %u\n", DEBUGGING_ANSWER);
	printf ("library bugreport = %s\n", PACKAGE_BUGREPORT);
	printf ("library name = %s\n", PACKAGE_NAME);
	printf ("library version = %s\n", PACKAGE_VERSION);
	printf ("syslog facility = %d\n", SYSLOG_FACILITY);
	printf ("build date = %s\n", library_info.build_date);
	printf ("build time = %s\n", library_info.build_time);
	printf ("compiler version = %s\n", library_info.compiler_version);
	printf ("database path = %s\n", DBPATH);

	/*
	 * The end.
	 */

	DEBUG ("end.");
	SUCCESS;

}

#undef FAILURE
#undef SUCCESS

