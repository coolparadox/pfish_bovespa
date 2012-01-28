/*
 * stock_list.c
 *
 * List of stocks in the pilot_fish bovespa database.
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

static char doc[] = "pfish_bovespa_stock_list -- list of stocks in the pilot_fish bovespa database.\vThis routine exports a list of stock identifiers through the standard output, one stock per line.\n";

static struct argp argp = { 0, 0, 0, doc };


/*
 * The portal.
 */

#define SUCCESS return (EXIT_SUCCESS)
#define FAILURE return (EXIT_FAILURE)

int main (int argc, char **argv) {

	pfish_bovespa_stock_list_t *stocks;	// Stock list.
	size_t i;	// General, short ranged indexer.

	/*
	 * Begin.
	 */

	pfish_syslog_init (SYSLOG_FACILITY, LOG_PERROR | LOG_PID);
	DEBUG ("start.");

	/*
	 * Parse command line arguments.
	 */

	argp_parse (&argp, argc, argv, 0, 0, 0);

	/*
	 * Retrieve the stock list from database.
	 */

	if ((stocks = pfish_bovespa_stock_list_alloc ()) == NULL) {

		CRIT ("canot retrieve stock list from database.");
		FAILURE;

	}

	/*
	 * Export stock list.
	 */

	for ( i = 0; i < stocks->stock_list_size; i++ ) {

		printf ("%s\n", stocks->stock_list[i].id);

	}

	/*
	 * Resource releasing.
	 */

	free (stocks);

	/*
	 * That was easy! :-)
	 */

	DEBUG ("end.");
	SUCCESS;

}

#undef FAILURE
#undef SUCCESS

