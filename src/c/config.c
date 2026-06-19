#include "../../include/config.h"

int load_config(const char *filename, Config *cfg){
    FILE *fp;
    char key[MAX_NAME_LEN];
    int value;

    if (filename == NULL || cfg == NULL) return -1;
    memset(cfg, 0, sizeof(Config));

    fp = fopen(filename, "r");
    if (fp == NULL) {
        perror ("Errore apertura file config");
        return -1;
    } 
    // questo while rappresenta il cuore della funzione:
    while (fscanf(fp, "%63s", key) == 1){
        if (key[0] == '#') {
            int ch;
            while ((ch = fgetc(fp)) != '\n' && ch != EOF);
            continue;
        }
        if (fscanf(fp, "%d", &value) != 1) continue;
        if (strcmp(key, "NOF_USERS") == 0) cfg->nof_users = value;
        else if (strcmp(key, "NOF_WORKERS") == 0) cfg->nof_workers = value;
        else if (strcmp(key, "SIM_DURATION") == 0) cfg->sim_duration = value;
        else if (strcmp(key, "OVERLOAD_THRESHOLD") == 0) cfg->overload_threshold = value;
        else if (strcmp(key, "NOF_WK_SEATS_PRIMI") == 0) cfg->nof_wk_seats_primi = value;
        else if (strcmp(key, "NOF_WK_SEATS_SECONDI") == 0) cfg->nof_wk_seats_secondi = value;
        else if (strcmp(key, "NOF_WK_SEATS_COFFEE") == 0) cfg->nof_wk_seats_coffee = value;
        else if (strcmp(key, "NOF_WK_SEATS_CASSA") == 0) cfg->nof_wk_seats_cassa = value;
        else if (strcmp(key, "NOF_TABLE_SEATS") == 0) cfg->nof_table_seats = value;
        else if (strcmp(key, "N_NANO_SECS") == 0) cfg->n_nano_secs = value;
        else if (strcmp(key, "AVG_SRVC_PRIMI") == 0) cfg->avg_srvc_primi = value;
        else if (strcmp(key, "AVG_SRVC_SECONDI") == 0) cfg->avg_srvc_secondi = value;
        else if (strcmp(key, "AVG_SRVC_COFFEE") == 0) cfg->avg_srvc_coffee = value;
        else if (strcmp(key, "AVG_SRVC_CASSA") == 0) cfg->avg_srvc_cassa = value;
        else if (strcmp(key, "AVG_REFILL_PRIMI") == 0) cfg->avg_refill_primi = value;
        else if (strcmp(key, "AVG_REFILL_SECONDI") == 0) cfg->avg_refill_secondi = value;
        else if (strcmp(key, "MAX_PORZIONI_PRIMI") == 0) cfg->max_porzioni_primi = value;
        else if (strcmp(key, "MAX_PORZIONI_SECONDI") == 0) cfg->max_porzioni_secondi = value;
        else if (strcmp(key, "NOF_PAUSE") == 0) cfg->nof_pause = value;
        else if (strcmp(key, "PRICE_PRIMI") == 0) cfg->price_primi = value;
        else if (strcmp(key, "PRICE_SECONDI") == 0) cfg->price_secondi = value;
        else if (strcmp(key, "PRICE_COFFEE") == 0) cfg->price_coffee = value;
        else if (strcmp(key, "STOP_DURATION") == 0) cfg->stop_duration = value;
        else if (strcmp(key, "N_NEW_USERS") == 0) cfg->n_new_users = value;
        else if (strcmp(key, "MAX_USERS_PER_GROUP") == 0) cfg->max_users_per_group = value;
        else if (strcmp(key, "AVG_REFILL_TIME") == 0) cfg->avg_refill_time = value;
	else if (strcmp(key, "MINUTI_GIORNATA") == 0) cfg->minuti_giornata = value;

    }

    // aggiungo questo if perchè se dopo aver letto tuttoil file avg_refill_time è ancora 0, 
    // lo imposto a 10 come valore di default, ciò evita che rifornimento usi un tempo di 0
    // min e faccia un loop infinito, occupando tutta la CPU.
    if (cfg->avg_refill_time == 0) cfg->avg_refill_time = 10;
    if (cfg->minuti_giornata == 0) cfg->minuti_giornata = 480;
fclose(fp);
return 0;
}

void print_config(const Config *cfg) {
    if (cfg == NULL) return;
    printf("--- CONFIGURAZIONE CARICATA ---\n");
    printf("NOF_USERS: %d\n", cfg->nof_users);
    printf("NOF_WORKERS: %d\n", cfg->nof_workers);
    printf("SIM_DURATION: %d giorni\n", cfg->sim_duration);
    printf("N_NANO_SECS: %ld\n", cfg->n_nano_secs);
    printf("NOF_TABLE_SEATS: %d\n", cfg->nof_table_seats);
    printf("AVG_SRVC_PRIMI: %d\n", cfg->avg_srvc_primi);
    printf("AVG_SRVC_SECONDI: %d\n", cfg->avg_srvc_secondi);
    printf("AVG_SRVC_COFFEE: %d\n", cfg->avg_srvc_coffee);
    printf("AVG_SRVC_CASSA: %d\n", cfg->avg_srvc_cassa);
    printf("PRICE_PRIMI: %d EUR\n", cfg->price_primi);
    printf("PRICE_SECONDI: %d EUR\n", cfg->price_secondi);
    printf("PRICE_COFFEE: %d EUR\n", cfg->price_coffee);
    printf("-------------------------------\n");

}

