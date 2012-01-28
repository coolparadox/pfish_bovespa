/*
 * stock_history.c
 *
 * Trade history of a stock of the pilot_fish bovespa database.
 */

#include <config.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
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

static char doc[] = "pfish_bovespa_stock_history -- trade history of a stock of the pilot_fish bovespa database.\vThis routine exports the trade history of STOCK through the standard output in CSV format.\n\nExported fields are: trading date, stock specification, price factor, opening price, closing price, minimum price, maximum price, average price, total trades, total stocks, total volume.\n\nFormat of date fields is YYYY-MM-DD.\nPrice and volume fields are in units of 1/100 of the stock currency.\n";

static char args_doc[] = "STOCK";

static struct argp_option options[] = {

	{"all", 'a', 0,  0, "show all trades (instead of starting in the most recent inplit / slit).", 0 },
	{ 0 }

};

struct arguments {

	unsigned int all;
	char *stock;

};


static error_t parse_opt (int key, char *arg, struct argp_state *state) {

	struct arguments *arguments = state->input;

	switch (key) {

		case 'a':

			arguments->all = 1;
			break;

		case ARGP_KEY_ARG:

			switch (state->arg_num) {

				case 0:
					arguments->stock = arg;
					break;

				default:

					argp_usage (state);

			}
			break;

		case ARGP_KEY_END:

			if (state->arg_num < 1) {

				argp_usage (state);

			}
			break;

		default:

			return ARGP_ERR_UNKNOWN;

	};

	return (0);

};

static struct argp argp = { options, parse_opt, args_doc, doc };


/* 
 * The portal. 
 */

#define SUCCESS return (EXIT_SUCCESS)
#define FAILURE return (EXIT_FAILURE)

#define DATE_BUF_SIZE 16

int main (int argc, char **argv) {

	struct arguments arguments;	// Arguments given in the command line.

	pfish_bovespa_stock_id_t stock_id;	// Stock identification.
	pfish_bovespa_stock_history_t *stock_history;	// Stock trade history.

	struct tm *trading_date;	// Time components of each trading date.
	char date_buf[DATE_BUF_SIZE];	// Trading date string formatting buffer.

	size_t i;	// General, short ranged indexer / counter.

	/*
	 * Begin.
	 */

	pfish_syslog_init (SYSLOG_FACILITY, LOG_PERROR | LOG_PID);
	DEBUG ("start.");

	/*
	 * Parse command line arguments.
	 */

	arguments.all = 0;
	arguments.stock = NULL;
	argp_parse (&argp, argc, argv, 0, 0, &arguments);
	if (arguments.stock == NULL) {

		CRIT ("missing stock identification.");
		FAILURE;

	}

	/*
	 * Retrieve stock history from the database.
	 */

	if ((strlen (arguments.stock)) > PFISH_BOVESPA_CODNEG_SIZE - 1) {

		CRIT ("stock name is too big.");
		FAILURE;

	}
	strcpy (stock_id.id, arguments.stock);
	if ((pfish_bovespa_stock_history_alloc (&stock_id, &stock_history)) < 0) {

		CRIT ("cannot retrieve history of stock '%s' from database.", stock_id.id);
		FAILURE;

	}
	if (stock_history == NULL) {

		ERR ("stock '%s' does not exist in database.", stock_id.id);
		FAILURE;

	}

	/*
	 * Export stock history.
	 */

	for ( i = ((arguments.all != 0) ? 0 : stock_history->last_xplit); i < stock_history->daily_quotes_size; i++ ) {

#define QUOTE stock_history->daily_quotes[i]

		if ((trading_date = gmtime (&(QUOTE.trading_date))) == NULL) {

			CRIT ("cannot understand trading date '%u' as a timestamp value.", QUOTE.trading_date);
			FAILURE;

		}
		if ((strftime (date_buf, DATE_BUF_SIZE, "%F", trading_date)) == 0) {

			CRIT ("cannot build the string representation of the trading date.");
			FAILURE;

		}
		printf (
			"%s,%s,%u,%Lu,%Lu,%Lu,%Lu,%Lu,%u,%Lu,%Lu\n",
			date_buf,
			QUOTE.stock_spec,
			QUOTE.price_factor,
			QUOTE.opening_price,
			QUOTE.closing_price,
			QUOTE.minimum_price,
			QUOTE.maximum_price,
			QUOTE.average_price,
			QUOTE.total_trades,
			QUOTE.total_stocks,
			QUOTE.total_volume
			);

#undef QUOTE

	}

	/*
	 * Resource releasing.
	 */

	if (pfish_bovespa_stock_history_free (stock_history)) {

		CRIT ("cannot release stock history.");
		FAILURE;

	}

	/*
	 * End.
	 */

	DEBUG ("end.");
	SUCCESS;

}

#undef DATE_BUF_SIZE

#undef FAILURE
#undef SUCCESS

