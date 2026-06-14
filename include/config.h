#ifndef CONFIG.H
#define CONFIG.H

#include "common.h"

#define MAX_LINE_LEN 256
#define MAX_NAME_LEN 64
#define MAX_MENU_ITEMS 16

typedef struct {
	int nof_users;
	int nof_workers;
	int sim_duration;
	int overload_threshold;
	int n_nano_secs;
	int avg_refill_time;
	int avg_refill_primi;
	int avg_refill_secondi;
	int max_porzioni_primi;
	int max_porzioni_secondi;
	int nof_table_seats;
	int nof_pause;
} Config;

int load_config(const char *filename, Config *cfg);

#endif
