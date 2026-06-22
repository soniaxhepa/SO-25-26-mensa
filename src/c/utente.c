#include "../../include/ipc.h"
#include "../../include/stats.h"
#include "../../include/config.h"



typedef struct {
    char nome[MAX_NAME_LEN];
} Piatto;

typedef struct {
    Piatto primi[MAX_MENU_ITEMS];   int n_primi;
    Piatto secondi[MAX_MENU_ITEMS]; int n_secondi;
    Piatto coffee[MAX_MENU_ITEMS];  int n_coffee;
} Menu;

static int sem_p_giornata(int sem_id, int sem_num, Stats *stats, int giorno) {
    struct sembuf s;
    struct timespec attesa = {0, 10000000L};
    s.sem_num = (unsigned short)sem_num;
    s.sem_op  = -1;
    s.sem_flg = IPC_NOWAIT;
    while (semop(sem_id, &s, 1) == -1) {
        if (errno != EAGAIN) return -1;
        sem_p(sem_id, SEM_MUTEX_STATS);
        int ng = stats->num_giorno;
        sem_v(sem_id, SEM_MUTEX_STATS);
        if (ng > giorno || stats->fine_giornata) return 1;
        nanosleep(&attesa, NULL);
    }
    return 0;
}


static int sem_piu_libero(int sem_id, int sem_a, int sem_b) {
    int va = semctl(sem_id, sem_a, GETVAL);
    int vb = semctl(sem_id, sem_b, GETVAL);
    return (va >= vb) ? sem_a : sem_b;
}


static void stat_add_int(int sem_id, int *campo, int delta) {
    sem_p(sem_id, SEM_MUTEX_STATS);
    *campo += delta;
    sem_v(sem_id, SEM_MUTEX_STATS);
}
static void stat_add_long(int sem_id, long *campo, long delta) {
    sem_p(sem_id, SEM_MUTEX_STATS);
    *campo += delta;
    sem_v(sem_id, SEM_MUTEX_STATS);
}


static int scegli_piatto_con_porzioni(int sem_id, int *porzioni,
                                       int n_piatti, int preferred) {
    int scelto = -1, j;
    sem_p(sem_id, SEM_MUTEX_STATS);
    if (porzioni[preferred] > 0) {
        porzioni[preferred]--;
        scelto = preferred;
    } else {
        for (j = 0; j < n_piatti; j++) {
            if (j != preferred && porzioni[j] > 0) {
                porzioni[j]--;
                scelto = j;
                break;
            }
        }
    }
    sem_v(sem_id, SEM_MUTEX_STATS);
    return scelto;
}

static int leggi_menu(const char *filename, Menu *menu) {
    FILE *f;
    char line[MAX_LINE_LEN], tipo[MAX_NAME_LEN], nome[MAX_NAME_LEN];
    menu->n_primi = menu->n_secondi = menu->n_coffee = 0;
    f = fopen(filename, "r");
    if (!f) { perror("[UTENTE] Errore apertura menu"); return -1; }
    while (fgets(line, sizeof(line), f)) {
        if (line[0] == '#' || line[0] == '\n') continue;
        if (sscanf(line, "%63s %63s", tipo, nome) != 2) continue;
        if (strcmp(tipo, "PRIMO") == 0 && menu->n_primi < MAX_MENU_ITEMS) {
            strncpy(menu->primi[menu->n_primi].nome, nome, MAX_NAME_LEN - 1);
            menu->primi[menu->n_primi].nome[MAX_NAME_LEN - 1] = '\0';
            menu->n_primi++;
        } else if (strcmp(tipo, "SECONDO") == 0 && menu->n_secondi < MAX_MENU_ITEMS) {
            strncpy(menu->secondi[menu->n_secondi].nome, nome, MAX_NAME_LEN - 1);
            menu->secondi[menu->n_secondi].nome[MAX_NAME_LEN - 1] = '\0';
            menu->n_secondi++;
        } else if (strcmp(tipo, "COFFEE") == 0 && menu->n_coffee < MAX_MENU_ITEMS) {
            strncpy(menu->coffee[menu->n_coffee].nome, nome, MAX_NAME_LEN - 1);
            menu->coffee[menu->n_coffee].nome[MAX_NAME_LEN - 1] = '\0';
            menu->n_coffee++;
        }
    }
    fclose(f);
    if (menu->n_primi < 2 || menu->n_secondi < 2 || menu->n_coffee < 4) {
        fprintf(stderr, "[UTENTE] menu insufficiente: 2 PRIMO, 2 SECONDO, 4 COFFEE\n");
        return -1;
    }
    return 0;
}

/* Sleep in minuti simulati. */
static void sleep_simulato(long minuti, long n_nano_secs) {
    struct timespec p;
    long ns = minuti * n_nano_secs;
    p.tv_sec  = ns / 1000000000L;
    p.tv_nsec = ns % 1000000000L;
    nanosleep(&p, NULL);
}



static int invia_e_attendi(int msg_id, Stats *stats,
                            long tipo, const ReqPayload *req, ResPayload *res) {
    struct message_t m;
    struct timespec attesa = {0, 10000000L};
    m.mtype = tipo;
    memcpy(m.mtext, req, sizeof(ReqPayload));
    if (msgsnd(msg_id, &m, MSG_LEN, 0) == -1) {
        perror("[UTENTE] msgsnd");
        return -1;
    }
    while (1) {
        if (msgrcv(msg_id, &m, MSG_LEN, (long)req->pid, IPC_NOWAIT) != -1) {
            memcpy(res, m.mtext, sizeof(ResPayload));
            return 0;
        }
        if (errno != ENOMSG) { perror("[UTENTE] msgrcv"); return -1; }
        //if (stats->simulazione_terminata) return 1;
        if (stats->simulazione_terminata || stats->fine_giornata) {
            struct message_t tmp;
            while (msgrcv(msg_id, &tmp, MSG_LEN, (long)req->pid, IPC_NOWAIT) != -1) {}
            return 1;
}
        nanosleep(&attesa, NULL);
    }
}

static int simula_giornata(int id, int sem_id, int msg_id, Stats *stats,
                            const Config *cfg, const Menu *menu,
                            int giorno, int ha_ticket,
                            int group_id, int group_size) {
    int n_piatti = 0, costo = 0, tipi_saltati = 0, i;
    struct timespec t_inizio, t_fine;
    long attesa_ms;

    int idx_primo    = rand() % menu->n_primi;
    int idx_secondo  = rand() % menu->n_secondi;
    int idx_coffee   = rand() % menu->n_coffee;
    int vuole_coffee = (rand() % 2);

    printf("[UTENTE %d] Menu: %s + %s%s%s\n", id,
           menu->primi[idx_primo].nome,
           menu->secondi[idx_secondo].nome,
           vuole_coffee ? " + caffe" : "",
           ha_ticket    ? " [TICKET -20%%]" : "");

    
    if (ha_ticket) {
        sem_p(sem_id, SEM_TICKET);
        printf("[UTENTE %d] Corsia ticket\n", id);
        sleep_simulato(1, cfg->n_nano_secs);
        sem_v(sem_id, SEM_TICKET);
    }

    
    int sem_first  = sem_piu_libero(sem_id, SEM_STAZIONE_PRIMI, SEM_STAZIONE_SECONDI);
    int sem_second = (sem_first == SEM_STAZIONE_PRIMI)
                     ? SEM_STAZIONE_SECONDI : SEM_STAZIONE_PRIMI;
    int ordine[2]  = {sem_first, sem_second};

    
    for (i = 0; i < 2; i++) {
        int  sem_staz   = ordine[i];
        long tipo_msg   = (sem_staz == SEM_STAZIONE_PRIMI) ? MSG_REQ_PRIMI : MSG_REQ_SECONDI;
        int  prezzo     = (sem_staz == SEM_STAZIONE_PRIMI) ? cfg->price_primi : cfg->price_secondi;
        int *stat_cnt   = (sem_staz == SEM_STAZIONE_PRIMI)
                          ? &stats->piatti_primi_totali : &stats->piatti_secondi_totali;
        long *stat_att  = (sem_staz == SEM_STAZIONE_PRIMI)
                          ? &stats->attesa_totale_primi : &stats->attesa_totale_secondi;
        const char *nome_staz = (sem_staz == SEM_STAZIONE_PRIMI) ? "PRIMI" : "SECONDI";
        int *porzioni_tipo    = (sem_staz == SEM_STAZIONE_PRIMI)
                                ? stats->porzioni_primi : stats->porzioni_secondi;
        int  n_tipo           = (sem_staz == SEM_STAZIONE_PRIMI)
                                ? menu->n_primi : menu->n_secondi;
        int  idx_preferito    = (sem_staz == SEM_STAZIONE_PRIMI)
                                ? idx_primo : idx_secondo;

        
        int piatto_scelto = scegli_piatto_con_porzioni(sem_id, porzioni_tipo,
                                                        n_tipo, idx_preferito);
        if (piatto_scelto == -1) {
            printf("[UTENTE %d] %s: tutti esauriti, salto\n", id, nome_staz);
            tipi_saltati++;
            continue;
        }
        if (piatto_scelto != idx_preferito) {
            const char *alt = (sem_staz == SEM_STAZIONE_PRIMI)
                              ? menu->primi[piatto_scelto].nome
                              : menu->secondi[piatto_scelto].nome;
            printf("[UTENTE %d] %s: preferito esaurito, prendo %s\n",
                   id, nome_staz, alt);
        }

        
        clock_gettime(CLOCK_MONOTONIC, &t_inizio);
        {
            ReqPayload req;
            ResPayload res;
            req.pid         = getpid();
            req.piatto      = piatto_scelto;
            req.prezzo_cent = 0;
            int r = invia_e_attendi(msg_id, stats, tipo_msg, &req, &res);
            if (r != 0) {
                printf("[UTENTE %d] %s: sim terminata\n", id, nome_staz);
                return 0;
            }
        }
        clock_gettime(CLOCK_MONOTONIC, &t_fine);

        
        attesa_ms = (t_fine.tv_sec  - t_inizio.tv_sec)  * 1000L
                  + (t_fine.tv_nsec - t_inizio.tv_nsec) / 1000000L;

        printf("[UTENTE %d] %s: servito in %ld ms\n", id, nome_staz, attesa_ms);
        stat_add_int (sem_id, stat_cnt,  1);
        stat_add_long(sem_id, stat_att,  attesa_ms);
        stat_add_long(sem_id, &stats->attesa_totale_complessiva, attesa_ms);
        costo += prezzo;
        n_piatti++;
    }

    if (tipi_saltati == 2) {
        printf("[UTENTE %d] Tutti i piatti esauriti, esco\n", id);
        return 0;
    }

    
    if (vuole_coffee) {
        clock_gettime(CLOCK_MONOTONIC, &t_inizio);
        ReqPayload req;
        ResPayload res;
        req.pid         = getpid();
        req.piatto      = idx_coffee;
        req.prezzo_cent = 0;
        if (invia_e_attendi(msg_id, stats, MSG_REQ_COFFEE, &req, &res) == 0) {
            clock_gettime(CLOCK_MONOTONIC, &t_fine);
            attesa_ms = (t_fine.tv_sec  - t_inizio.tv_sec)  * 1000L
                      + (t_fine.tv_nsec - t_inizio.tv_nsec) / 1000000L;
            printf("[UTENTE %d] Coffee (%s): %ld ms\n",
                   id, menu->coffee[idx_coffee].nome, attesa_ms);
            stat_add_int (sem_id, &stats->piatti_coffee_totali,     1);
            stat_add_long(sem_id, &stats->attesa_totale_coffee,      attesa_ms);
            stat_add_long(sem_id, &stats->attesa_totale_complessiva, attesa_ms);
            costo += cfg->price_coffee;
            n_piatti++;
        }
    }

    
    // BARRIERA DI GRUPPO PRE-CASSA 
    
    if (group_id >= 0 && group_size > 1) {
        int count;
        sem_p(sem_id, SEM_MUTEX_STATS);
        stats->gruppi_pronti[group_id]++;
        count = stats->gruppi_pronti[group_id] % group_size;
        sem_v(sem_id, SEM_MUTEX_STATS);

        if (count == 0) {
            int k;
            printf("[UTENTE %d] Ultimo del gruppo %d, sblocco %d\n",
                   id, group_id, group_size - 1);
            for (k = 0; k < group_size - 1; k++)
                sem_v(sem_id, SEM_GRUPPO_BASE + group_id);
        } else {
            printf("[UTENTE %d] Attendo gruppo %d (%d/%d)\n",
                   id, group_id, count, group_size);
            if (sem_p_giornata(sem_id, SEM_GRUPPO_BASE + group_id,
                                stats, giorno) != 0) {
                printf("[UTENTE %d] Barriera gruppo: giornata finita\n", id);
                return 0;
            }
            printf("[UTENTE %d] Gruppo %d completo, vado alla cassa\n",
                   id, group_id);
        }
    }

    //Sconto ticket: 80% del totale
    if (ha_ticket) {
        int pieno = costo;
        costo = costo * 80 / 100;
        printf("[UTENTE %d] Ticket: %d → %d\n", id, pieno, costo);
    }

    //CASSA 
    clock_gettime(CLOCK_MONOTONIC, &t_inizio);
    {
        ReqPayload req;
        ResPayload res;
        req.pid         = getpid();
        req.piatto      = 0;
        req.prezzo_cent = costo;
        if (invia_e_attendi(msg_id, stats, MSG_REQ_CASSA, &req, &res) != 0) {
            printf("[UTENTE %d] Cassa: sim terminata\n", id);
            return 0;
        }
    }
    clock_gettime(CLOCK_MONOTONIC, &t_fine);
    attesa_ms = (t_fine.tv_sec  - t_inizio.tv_sec)  * 1000L
              + (t_fine.tv_nsec - t_inizio.tv_nsec) / 1000000L;
    printf("[UTENTE %d] Cassa: pago %d, atteso %ld ms\n", id, costo, attesa_ms);
    stat_add_long(sem_id, &stats->attesa_totale_cassa,       attesa_ms);
    stat_add_long(sem_id, &stats->attesa_totale_complessiva, attesa_ms);

    // TAVOLI
    if (sem_p_giornata(sem_id, SEM_TAVOLI, stats, giorno) != 0) {
        printf("[UTENTE %d] Tavolo: giornata finita\n", id);
        return 0;
    }
    printf("[UTENTE %d] Tavolo: seduto, consumo %d piatti\n", id, n_piatti);
    sleep_simulato((long)n_piatti, cfg->n_nano_secs);
    sem_v(sem_id, SEM_TAVOLI);
    printf("[UTENTE %d] Tavolo: libero\n", id);
    return 1;
}

//Main
int main(int argc, char *argv[]) {
    int    shm_id, sem_id, msg_id;
    Stats *stats;
    Config cfg;
    Menu   menu;
    int    id, giorno, nuovo, ha_ticket, group_id, group_size;
    struct sembuf sops;

    if (argc < 8) {
        fprintf(stderr,
            "Uso: %s <id> <config> <menu> <nuovo> <ticket> <group_id> <group_size>\n",
            argv[0]);
        return 1;
    }
    id         = atoi(argv[1]);
    nuovo      = atoi(argv[4]);
    ha_ticket  = atoi(argv[5]);
    group_id   = atoi(argv[6]);
    group_size = atoi(argv[7]);

    printf("[UTENTE %d] PID=%d, nuovo=%d, ticket=%d, gruppo=%d(sz=%d)\n",
           id, (int)getpid(), nuovo, ha_ticket, group_id, group_size);

    if (load_config(argv[2], &cfg) != 0) {
        fprintf(stderr, "[UTENTE %d] Errore config\n", id); return 1;
    }
    if (leggi_menu(argv[3], &menu) != 0) {
        fprintf(stderr, "[UTENTE %d] Errore menu\n", id); return 1;
    }

    shm_id = shmget(SHM_KEY, sizeof(Stats), 0);
    if (shm_id == -1) { perror("[UTENTE] shmget"); return 1; }

    stats = (Stats *)shmat(shm_id, NULL, 0);
    if (stats == (Stats *)-1) { perror("[UTENTE] shmat"); return 1; }

    sem_id = semget(SEM_KEY, 0, 0);
    if (sem_id == -1) { perror("[UTENTE] semget"); shmdt(stats); return 1; }

    msg_id = msgget(MSG_KEY, 0);
    if (msg_id == -1) { perror("[UTENTE] msgget"); shmdt(stats); return 1; }

    srand((unsigned int)getpid());

    /*
     * Barriera iniziale: ogni utente normale decrementa SEM_MUTEX_SYNC
     * poi aspetta che arrivi a 0 (tutti pronti: utenti + operatori + rifornimento).
     * Utenti nuovi (aggiunti a sim in corso) saltano la barriera.
     */
    if (nuovo == 0) {
        sops.sem_num = SEM_MUTEX_SYNC; sops.sem_op = -1; sops.sem_flg = 0;
        if (semop(sem_id, &sops, 1) == -1) {
            perror("[UTENTE] semop sync -1"); shmdt(stats); return 1;
        }
        printf("[UTENTE %d] Pronto, attendo apertura mensa...\n", id);
        sops.sem_op = 0;
        if (semop(sem_id, &sops, 1) == -1) {
            perror("[UTENTE] semop sync 0"); shmdt(stats); return 1;
        }
    } else {
        printf("[UTENTE %d] Nuovo utente, entro direttamente\n", id);
    }

    printf("[UTENTE %d] Mensa aperta! (%d giorni)\n", id, cfg.sim_duration);

    // Loop giornate 
    for (giorno = 1; giorno <= cfg.sim_duration; giorno++) {

        // Controlla overload prima di iniziare la giornata 
        sem_p(sem_id, SEM_MUTEX_STATS);
        int terminata = stats->simulazione_terminata;
        sem_v(sem_id, SEM_MUTEX_STATS);
        if (terminata) break;

        // Aspetta il via del responsabile 
        sops.sem_num = SEM_INIZIO_GIORNATA; sops.sem_op = -1; sops.sem_flg = 0;
        semop(sem_id, &sops, 1);
        printf("[UTENTE %d] === Giorno %d ===\n", id, giorno);
        
        // Controlla se la giornata è già conclusa 
        sem_p(sem_id, SEM_MUTEX_STATS);
        int ng = stats->num_giorno;
        sem_v(sem_id, SEM_MUTEX_STATS);
        if (ng > giorno) {
            stat_add_int(sem_id, &stats->utenti_non_serviti_totali, 1);
            printf("[UTENTE %d] Giorno %d: già concluso\n", id, giorno);
            sops.sem_num = SEM_FINE_GIORNATA; sops.sem_op = 1;
            semop(sem_id, &sops, 1);
            continue;
        }

        int servito = simula_giornata(id, sem_id, msg_id, stats, &cfg, &menu,
                                      giorno, ha_ticket, group_id, group_size);
        if (servito) {
            stat_add_int(sem_id, &stats->utenti_serviti_totali, 1);
            printf("[UTENTE %d] Giorno %d: servito\n", id, giorno);
        } else {
            stat_add_int(sem_id, &stats->utenti_non_serviti_totali, 1);
            // Overload: se troppi non serviti, segnala stop 
            sem_p(sem_id, SEM_MUTEX_STATS);
            if (stats->utenti_non_serviti_totali >= cfg.overload_threshold
                    && stats->simulazione_terminata == 0) {
                stats->simulazione_terminata = 1;
                stats->causa_terminazione = TERM_OVERLOAD;
                fprintf(stderr, "[UTENTE %d] OVERLOAD!\n", id);
            }
            sem_v(sem_id, SEM_MUTEX_STATS);
            printf("[UTENTE %d] Giorno %d: non servito\n", id, giorno);
        }

        // Segnala fine giornata al responsabile 
        sops.sem_num = SEM_FINE_GIORNATA; sops.sem_op = 1; sops.sem_flg = 0;
        semop(sem_id, &sops, 1);
    }

    printf("[UTENTE %d] Fine simulazione\n", id);
    shmdt(stats);
    return 0;
}
