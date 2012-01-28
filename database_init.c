/*
 * database_init.c
 *
 * Initialize the Bovespa database.
 */

#include <config.h>

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <syslog.h>
#include <argp.h>
#include <sys/stat.h>

#include <pilot_fish/syslog.h>
#include <pilot_fish/syslog_macros.h>
#include <pilot_fish/bovespa.h>

#include "revision_marker.h"


/*
 * Command line argument parsing.
 */

const char *argp_program_version = PACKAGE_VERSION;
const char *argp_program_bug_address = PACKAGE_BUGREPORT;

static char doc[] = "pfish_bovespa_database_init -- cleanup and initialization of pilot_fish bovespa database.\vThis routine wipes out any previously existent database files, and initializes the database working path.\n";

static struct argp_option options[] = {

	{"interactive", 'i', 0, 0, "prompt for confirmation prior to erase things (default).", 0 },
	{"force", 'f', 0, 0, "erase previously existent database files without confirmation.", 0 },
	{ 0 }

};

struct arguments {

	int interactive;

};

static error_t parse_opt (int key, char *arg, struct argp_state *state) {

	struct arguments *arguments = state->input;

	switch (key) {

		case 'i':

			arguments->interactive = 1;
			break;

		case 'f':

			arguments->interactive = 0;
			break;

		case ARGP_KEY_ARG:

			argp_usage (state);
			break;

		/*
		case ARGP_KEY_END:

			break;
		*/

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
	struct stat dbpath_stat;	// Investigation about the database path.
	int dbpath_stat_rcode;	// Return code of stat on the database path.
	pfish_bovespa_library_info_t library_info;	// Database revision marker uses library information.
	char *revision_marker_content;	// Expected content of revision marker file.
	FILE *revision_marker_file;	// Stream to manipulate the revision marker file.

#define PROMPT_ANSWER_SIZE 5

	char prompt_answer[PROMPT_ANSWER_SIZE];	// User's answer to the 'are you sure?' prompt.

	pfish_syslog_init (SYSLOG_FACILITY, LOG_PERROR);
	DEBUG ("start.");

	/*
	 * Parse command line arguments.
	 */

	arguments.interactive = 1;
	argp_parse (&argp, argc, argv, 0, 0, &arguments);

	/*
	 * Investigate the database path.
	 */

	if ((dbpath_stat_rcode = stat (DBPATH, &dbpath_stat)) < 0) {

		switch (errno) {

			case ENOENT:

				/*
				 * Database directory does not exist; we're cool.
				 */

				break;

			default:

				ERRNO_ERR;
				CRIT ("cannot stat database path '%s'.", DBPATH);
				FAILURE;

		}

	}

	/*
	 * Check the need for interaction.
	 */

	if ((dbpath_stat_rcode >= 0) && (arguments.interactive != 0)) {

		/*
		 * Prompt for confirmation before going on.
		 */

		if ((isatty (STDIN_FILENO)) != 1) {

			ERR ("cannot request user confirmation (not a tty).");
			FAILURE;

		}
		WARNING ("existent database directory detected.");
		fflush (stderr);
		printf ("Initialization will erase current database files, are you sure (yes/no)? ");
		fflush (stdout);
		if ((fgets (prompt_answer, PROMPT_ANSWER_SIZE, stdin)) == NULL) {

			CRIT ("cannot prompt user for confirmation.");
			FAILURE;

		}
		if ((strcmp (prompt_answer, "yes\n")) != 0) {

			WARNING ("user gave up.");
			FAILURE;

		}

	}

	/*
	 * Cleanup database working area.
	 */

	NOTICE ("initializing an empty database.");
	if ((system ("/bin/rm -rf " DBPATH "/*")) != 0) {

		CRIT ("cannot clean database directory '%s'.", DBPATH);
		FAILURE;

	}
	if ((system ("/bin/mkdir -p " DBPATH)) != 0) {

		CRIT ("cannot create database directory '%s'.", DBPATH);
		FAILURE;

	}

	/*
	 * Initialize database revision marker.
	 */

	if ((pfish_bovespa_revision_marker_content_alloc (&revision_marker_content)) < 0) {

		CRIT ("cannot build contents of revision marker file.");
		FAILURE;

	}
	if ((revision_marker_file = fopen (REVISION_MARKER_PATHNAME, "w")) == NULL) {

		CRIT ("cannot open revision file marker in write mode.");
		FAILURE;

	}
	if ((fprintf (revision_marker_file, "%s", revision_marker_content)) < 0) {

		CRIT ("cannot write content to revision marker file.");
		FAILURE;

	}
	if ((fclose (revision_marker_file)) != 0) {

		CRIT ("cannot close revision marker file.");
		FAILURE;

	}
	free (revision_marker_content);

	/*
	 * End.
	 */

	INFO ("database path '%s' cleaned and initialized.", DBPATH);
	SUCCESS;

}

#undef FAILURE
#undef SUCCESS

