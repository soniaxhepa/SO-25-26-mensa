#ifndef CONFIG.H
#define CONFIG.H

#include "common.h"

#define MAX_LINE_LEN 256
#define MAX_NAME_LEN 64
#define MAX_MENU_ITEMS 16

typedef struct {
	int nof_users;
	int nof_workers;

	int nof_wk_seats_primi;
        int nof_wk_seats_secondi;
        int nof_wk_seats_coffee;
        int nof_wk_seats_cassa;

	int sim_duration;
	int overload_threshold;
	long n_nano_secs;

	int avg_refill_time;
	int avg_refill_primi;
	int avg_refill_secondi;

	int max_porzioni_primi;
	int max_porzioni_secondi;

	int nof_table_seats;
	int nof_pause;
	
	int avg_srvc_primi;
        int avg_srvc_secondi;
        int avg_srvc_coffee;
        int avg_srvc_cassa;
       
        int price_primi;
        int price_secondi;
        int price_coffee;
        int stop_duration;
        int n_new_users;
        int max_users_per_group;
} Config;

int load_config(const char *filename, Config *cfg);

void print_config(const Config *cfg);
#endif
