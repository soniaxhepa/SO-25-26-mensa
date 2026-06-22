#include "../../include/stats.h"

void init_stats(Stats *stats) {
    if (stats == NULL) return;
    memset(stats, 0, sizeof(Stats));
}


void print_stats(const Stats *stats) {
    if (stats == NULL) return;
    int ng = stats->num_giorno > 0 ? stats->num_giorno : 1;
    int pt = stats->piatti_primi_totali + stats->piatti_secondi_totali + stats->piatti_coffee_totali;
    int s  = stats->utenti_serviti_totali > 0 ? stats->utenti_serviti_totali : 1;
    int sp = stats->piatti_primi_totali   > 0 ? stats->piatti_primi_totali   : 1;
    int ss = stats->piatti_secondi_totali > 0 ? stats->piatti_secondi_totali : 1;
    int sc = stats->piatti_coffee_totali  > 0 ? stats->piatti_coffee_totali  : 1;

    printf("\n========== REPORT FINALE ==========\n");

    printf("\n--- UTENTI ---\n");
    printf("Serviti totali:           %d\n",   stats->utenti_serviti_totali);
    printf("Non serviti totali:       %d\n",   stats->utenti_non_serviti_totali);
    printf("Serviti media/giorno:     %d\n",   stats->utenti_serviti_totali / ng);
    printf("Non serviti media/giorno: %d\n",   stats->utenti_non_serviti_totali / ng);

    printf("\n--- PIATTI DISTRIBUITI ---\n");
    printf("Totali:           %d (media/giorno: %d)\n", pt, pt / ng);
    printf("Primi:            %d (media/giorno: %d)\n", stats->piatti_primi_totali,   stats->piatti_primi_totali / ng);
    printf("Secondi:          %d (media/giorno: %d)\n", stats->piatti_secondi_totali, stats->piatti_secondi_totali / ng);
    printf("Coffee:           %d (media/giorno: %d)\n", stats->piatti_coffee_totali,  stats->piatti_coffee_totali / ng);

    printf("\n--- PIATTI AVANZATI ---\n");
    printf("Primi:   %d (media/giorno: %d)\n", stats->avanzi_primi_totali,   stats->avanzi_primi_totali / ng);
    printf("Secondi: %d (media/giorno: %d)\n", stats->avanzi_secondi_totali, stats->avanzi_secondi_totali / ng);
    printf("Coffee:  illimitati/non conteggiati\n");

    printf("\n--- TEMPI MEDI DI ATTESA (ms/utente) ---\n");
    printf("Primi:       %ld\n", stats->attesa_totale_primi       / sp);
    printf("Secondi:     %ld\n", stats->attesa_totale_secondi     / ss);
    printf("Coffee:      %ld\n", stats->attesa_totale_coffee      / sc);
    printf("Cassa:       %ld\n", stats->attesa_totale_cassa       / s);
    printf("Complessivo: %ld\n", stats->attesa_totale_complessiva / s);

    printf("\n--- OPERATORI ---\n");
    printf("Operatori attivi:   %d\n",   stats->operatori_attivi_totali);
    printf("Pause totali:       %d\n",   stats->pause_totali);
    printf("Pause media/giorno: %.1f\n", (float)stats->pause_totali / ng);

    printf("\n--- RICAVO ---\n");
    printf("Ricavo totale:       %d EUR\n", stats->ricavo_totale);
    printf("Ricavo media/giorno: %d EUR\n", stats->ricavo_totale / ng);

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

    stats->snapshot_piatti_primi   = stats->piatti_primi_totali;
    stats->snapshot_piatti_secondi = stats->piatti_secondi_totali;
    stats->snapshot_piatti_coffee  = stats->piatti_coffee_totali;
}

void print_stats_giornata(Stats *stats, int giorno) {
    if (stats == NULL) return;

    int serviti_oggi     = stats->utenti_serviti_totali     - stats->snapshot_utenti_serviti;
    int non_serviti_oggi = stats->utenti_non_serviti_totali - stats->snapshot_utenti_non_serviti;
    int primi_oggi   = stats->piatti_primi_totali   - stats->snapshot_piatti_primi;
    int secondi_oggi = stats->piatti_secondi_totali - stats->snapshot_piatti_secondi;
    int coffee_oggi  = stats->piatti_coffee_totali  - stats->snapshot_piatti_coffee;
    int piatti_oggi  = primi_oggi + secondi_oggi + coffee_oggi;
    long att_primi_oggi = stats->attesa_totale_primi       - stats->snapshot_attesa_primi;
    long att_sec_oggi   = stats->attesa_totale_secondi     - stats->snapshot_attesa_secondi;
    long att_cof_oggi   = stats->attesa_totale_coffee      - stats->snapshot_attesa_coffee;
    long att_cas_oggi   = stats->attesa_totale_cassa       - stats->snapshot_attesa_cassa;
    long att_tot_oggi   = stats->attesa_totale_complessiva - stats->snapshot_attesa_complessiva;
    int ricavo_oggi = stats->ricavo_totale - stats->snapshot_ricavo;
    int pause_oggi  = stats->pause_totali  - stats->snapshot_pause;
    int op_attivi   = stats->operatori_attivi_totali;

    printf("[GIORNO %d] Utenti serviti: %d | Non serviti: %d\n",
           giorno, serviti_oggi, non_serviti_oggi);
    printf("[GIORNO %d] Piatti: tot=%d | primi=%d | secondi=%d | coffee=%d\n",
           giorno, piatti_oggi, primi_oggi, secondi_oggi, coffee_oggi);
    printf("[GIORNO %d] Attese medie (ms) — primi: %ld | secondi: %ld | coffee: %ld | cassa: %ld | tot: %ld\n",
           giorno,
           primi_oggi   > 0 ? att_primi_oggi / primi_oggi   : 0L,
           secondi_oggi > 0 ? att_sec_oggi   / secondi_oggi : 0L,
           coffee_oggi  > 0 ? att_cof_oggi   / coffee_oggi  : 0L,
           serviti_oggi > 0 ? att_cas_oggi   / serviti_oggi : 0L,
           serviti_oggi > 0 ? att_tot_oggi   / serviti_oggi : 0L);
    printf("[GIORNO %d] Operatori attivi: %d | Pause: %d (media/op: %.1f)\n",
           giorno, op_attivi, pause_oggi,
           op_attivi > 0 ? (float)pause_oggi / op_attivi : 0.0f);
    printf("[GIORNO %d] Ricavo: %d EUR\n", giorno, ricavo_oggi);
}
