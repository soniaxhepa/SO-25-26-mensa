#ifndef STATS_H
#define STATS_H

#include "common.h"
#include "config.h"
#include "ipc.h"

typedef struct {
    int utenti_serviti_totali;
    int utenti_non_serviti_totali;

   
    int piatti_primi_totali;
    int piatti_secondi_totali;
    int piatti_coffee_totali;

  
    int avanzi_primi_totali;
    int avanzi_secondi_totali;
    int avanzi_coffee_totali;


    int porzioni_primi[MAX_MENU_ITEMS];
    int porzioni_secondi[MAX_MENU_ITEMS];

   
    long attesa_totale_primi;
    long attesa_totale_secondi;
    long attesa_totale_coffee;
    long attesa_totale_cassa;
    long attesa_totale_complessiva;

    int operatori_attivi_totali;
    int pause_totali;

 
    int ricavo_totale;
    int simulazione_terminata;
    int causa_terminazione;
    int giorno_rifornito;
    int n_in_attesa_gruppo;
    int fine_giornata;

    int gruppi_size[MAX_GRUPPI_SEM];
    int gruppi_pronti[MAX_GRUPPI_SEM];
    int n_gruppi;

    int num_giorno;
    int snapshot_utenti_serviti;
    int snapshot_piatti_totali;

} Stats;

void init_stats(Stats *stats);

void print_stats(const Stats *stats);

void salva_stats_csv(const Stats *stats, const char *filename);

void scatta_snapshot(Stats *stats);

void print_stats_giornata(Stats *stats, int giorno);

#endif
