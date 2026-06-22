#include "../../include/stats.h"

void init_stats(Stats *stats) {
    if (stats == NULL) return;
    memset(stats, 0, sizeof(Stats));
}


void print_stats(const Stats *stats) {
    if (stats == NULL) return;
    printf("\n========== REPORT FINALE ==========\n");

    printf("\n--- UTENTI ---\n");
    printf("Serviti totali:     %d\n", stats->utenti_serviti_totali);
    printf("Non serviti totali: %d\n", stats->utenti_non_serviti_totali);

    printf("\n--- PIATTI DISTRIBUITI ---\n");
    printf("Primi:   %d\n", stats->piatti_primi_totali);
    printf("Secondi: %d\n", stats->piatti_secondi_totali);
    printf("Coffee:  %d\n", stats->piatti_coffee_totali);

    printf("\n--- PIATTI AVANZATI ---\n");
    printf("Avanzi primi:   %d\n", stats->avanzi_primi_totali);
    printf("Avanzi secondi: %d\n", stats->avanzi_secondi_totali);
    printf("Avanzi coffee:  illimitati/non conteggiati\n");

    printf("\n--- TEMPI DI ATTESA (secondi) ---\n");
    printf("Attesa primi:        %ld\n", stats->attesa_totale_primi);
    printf("Attesa secondi:      %ld\n", stats->attesa_totale_secondi);
    printf("Attesa coffee:       %ld\n", stats->attesa_totale_coffee);
    printf("Attesa cassa:        %ld\n", stats->attesa_totale_cassa);
    printf("Attesa complessiva:  %ld\n", stats->attesa_totale_complessiva);

    printf("\n--- OPERATORI ---\n");
    printf("Operatori attivi: %d\n", stats->operatori_attivi_totali);
    printf("Pause totali:     %d\n", stats->pause_totali);

    printf("\n--- RICAVO ---\n");
    printf("Ricavo totale: %d EUR\n", stats->ricavo_totale);

    printf("\n--- TERMINAZIONE ---\n");
    if (stats->causa_terminazione == TERM_TIMEOUT)
        printf("Causa: TIMEOUT\n");
    else if (stats->causa_terminazione == TERM_OVERLOAD)
        printf("Causa: OVERLOAD\n");

    printf("====================================\n\n");
}

//aggiungo l'ultima funzione per la versione completa da 30/30
void salva_stats_csv(const Stats *stats, const char *filename) {
    if (stats == NULL || filename == NULL) return;
    FILE *f = fopen(filename, "w");
    if (f == NULL) { perror("[STATS] Errore apertura CSV"); return; }
        
    fprintf(f, "campo,valore\n");
    fprintf(f, "utenti_serviti,%d\n",      stats->utenti_serviti_totali);
    fprintf(f, "utenti_non_serviti,%d\n",  stats->utenti_non_serviti_totali);
    fprintf(f, "piatti_primi,%d\n",        stats->piatti_primi_totali);
    fprintf(f, "piatti_secondi,%d\n",      stats->piatti_secondi_totali);
    fprintf(f, "piatti_coffee,%d\n",       stats->piatti_coffee_totali);
    fprintf(f, "avanzi_primi,%d\n",        stats->avanzi_primi_totali);
    fprintf(f, "avanzi_secondi,%d\n",      stats->avanzi_secondi_totali);
    fprintf(f, "avanzi_coffee,%d\n",       stats->avanzi_coffee_totali);
    fprintf(f, "attesa_primi,%ld\n",       stats->attesa_totale_primi);
    fprintf(f, "attesa_secondi,%ld\n",     stats->attesa_totale_secondi);
    fprintf(f, "attesa_coffee,%ld\n",      stats->attesa_totale_coffee);
    fprintf(f, "attesa_cassa,%ld\n",       stats->attesa_totale_cassa);
    fprintf(f, "attesa_totale,%ld\n",      stats->attesa_totale_complessiva);
    fprintf(f, "operatori_attivi,%d\n",    stats->operatori_attivi_totali);
    fprintf(f, "pause_totali,%d\n",        stats->pause_totali);    
    fprintf(f, "ricavo_totale,%d\n",       stats->ricavo_totale);
    
    fclose(f);
    printf("[STATS] Statistiche salvate in %s\n", filename);
}

void scatta_snapshot(Stats *stats) {
    if (stats == NULL) return;
    stats->snapshot_utenti_serviti      = stats->utenti_serviti_totali;
    stats->snapshot_utenti_non_serviti  = stats->utenti_non_serviti_totali;
    stats->snapshot_piatti_totali       = stats->piatti_primi_totali
                                        + stats->piatti_secondi_totali
                                        + stats->piatti_coffee_totali;
    stats->snapshot_attesa_primi        = stats->attesa_totale_primi;
    stats->snapshot_attesa_secondi      = stats->attesa_totale_secondi;
    stats->snapshot_attesa_coffee       = stats->attesa_totale_coffee;
    stats->snapshot_attesa_cassa        = stats->attesa_totale_cassa;
    stats->snapshot_attesa_complessiva  = stats->attesa_totale_complessiva;
    stats->snapshot_ricavo              = stats->ricavo_totale;
    stats->snapshot_pause               = stats->pause_totali;
}

void print_stats_giornata(Stats *stats, int giorno) {
    if (stats == NULL) return;

    int serviti_oggi     = stats->utenti_serviti_totali     - stats->snapshot_utenti_serviti;
    int non_serviti_oggi = stats->utenti_non_serviti_totali - stats->snapshot_utenti_non_serviti;
    int piatti_oggi      = (stats->piatti_primi_totali + stats->piatti_secondi_totali
                            + stats->piatti_coffee_totali)  - stats->snapshot_piatti_totali;
    long att_primi_oggi  = stats->attesa_totale_primi       - stats->snapshot_attesa_primi;
    long att_sec_oggi    = stats->attesa_totale_secondi     - stats->snapshot_attesa_secondi;
    long att_cof_oggi    = stats->attesa_totale_coffee      - stats->snapshot_attesa_coffee;
    long att_cas_oggi    = stats->attesa_totale_cassa       - stats->snapshot_attesa_cassa;
    long att_tot_oggi    = stats->attesa_totale_complessiva - stats->snapshot_attesa_complessiva;
    int ricavo_oggi      = stats->ricavo_totale             - stats->snapshot_ricavo;
    int pause_oggi       = stats->pause_totali              - stats->snapshot_pause;

    printf("[GIORNO %d] Utenti serviti: %d | Non serviti: %d\n",
           giorno, serviti_oggi, non_serviti_oggi);
    printf("[GIORNO %d] Piatti erogati: %d\n", giorno, piatti_oggi);
    printf("[GIORNO %d] Attese (ms) — primi: %ld | secondi: %ld | coffee: %ld | cassa: %ld | tot: %ld\n",
           giorno, att_primi_oggi, att_sec_oggi, att_cof_oggi, att_cas_oggi, att_tot_oggi);
    printf("[GIORNO %d] Pause effettuate: %d | Ricavo: %d EUR\n",
           giorno, pause_oggi, ricavo_oggi);
}
