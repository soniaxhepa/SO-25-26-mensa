// Processo principale che avvia la simulazione
#include "responsabile_mensa.h"
#include <signal.h>

static int    g_shm_id         = -1;
static int    g_sem_id         = -1;
static int    g_msg_id         = -1;
static Stats *g_stats          = NULL;
static int    g_nof_workers    = 0;
static int    g_n_gruppi       = 0;
static int    g_n_utenti       = 0;
static pid_t  g_pids_utenti[1024];
static pid_t  g_pids_operatori[64];

// handle signal: gestisce i segnali SIGINT e SIGTERM
static void handle_signal(int sig) {
    int g, k;
    struct sembuf sops;
    (void)sig;

    // Avviso i processi che la simulazione è terminata
    if (g_stats != NULL && g_sem_id != -1) {
        sops.sem_num = SEM_MUTEX_STATS;
        sops.sem_flg = 0;
        sops.sem_op  = -1;
        if (semop(g_sem_id, &sops, 1) != -1) {
            g_stats->simulazione_terminata = 1;
            g_stats->fine_giornata         = 1;
            sops.sem_op = 1;
            semop(g_sem_id, &sops, 1);
        }

        // Sblocco eventuali utenti rimasti bloccati
        for (g = 0; g < g_n_gruppi; g++) {
            int waiting = 0;
            sops.sem_num = SEM_MUTEX_STATS; sops.sem_flg = 0;
            sops.sem_op  = -1; semop(g_sem_id, &sops, 1);
            if (g_stats->gruppi_size[g] > 1)
                waiting = g_stats->gruppi_pronti[g] % g_stats->gruppi_size[g];
            sops.sem_op = 1; semop(g_sem_id, &sops, 1);

            for (k = 0; k < waiting; k++) {
                struct sembuf v;
                v.sem_num = (unsigned short)(SEM_GRUPPO_BASE + g);
                v.sem_flg = 0;
                v.sem_op  = 1;
                semop(g_sem_id, &v, 1);
            }
        }
    }


    // Sblocco eventuali operatori bloccati
    if (g_sem_id != -1 && g_nof_workers > 0) {
        struct sembuf v;
        v.sem_num = SEM_FINE_SIM;
        v.sem_flg = 0;
        v.sem_op  = 1;
        for (k = 0; k < g_nof_workers; k++)
            semop(g_sem_id, &v, 1);
    }

    // Pulizia finale
    if (g_stats != NULL) {
        shmdt(g_stats);
        g_stats = NULL;
    }
    if (g_shm_id != -1 && g_sem_id != -1)
        destroy_ipc(g_shm_id, g_sem_id);
    if (g_msg_id != -1)
        msgctl(g_msg_id, IPC_RMID, NULL);

    _exit(EXIT_SUCCESS);
}

// CREO UTENTI
static int crea_utenti_exec(int n_utenti, const char *config_file, const char *menu_file, int max_group, Stats *stats) {
    extern char **environ;
    int i, g;
    pid_t pid;
    int group_id[1024];
    int group_size_arr[1024];

    if (n_utenti > 1024) {
        fprintf(stderr, "[RESPONSABILE MENSA] Troppi utenti (max 1024)\n");
        return -1;
    }

    // Assegno i gruppi a tutti gli utenti prima di fare fork()
    g = 0;
    i = 0;
    while (i < n_utenti) {
        int size, j;
        if (g >= MAX_GRUPPI_SEM) {
            size = 1;
        } else {
            size = 1 + rand() % max_group;
            if (i + size > n_utenti)
                size = n_utenti - i;
        }
        for (j = 0; j < size; j++) {
            group_id[i + j]       = g;
            group_size_arr[i + j] = size;
        }
        stats->gruppi_size[g]   = size;
        stats->gruppi_pronti[g] = 0;
        i += size;
        g++;
    }
    stats->n_gruppi = g;
    g_n_gruppi      = g;
    g_n_utenti      = n_utenti;

    // fork() + execve() per ogni utente
    for (i = 0; i < n_utenti; i++) {
        switch (pid = fork()) {
            case -1:
                perror("[RESPONSABILE MENSA] Errore fork utente");
                return -1;
            case 0: {
                char idx_str[16], gid_str[16], gsz_str[16];
                snprintf(idx_str, sizeof(idx_str), "%d", i);
                snprintf(gid_str, sizeof(gid_str), "%d", group_id[i]);
                snprintf(gsz_str, sizeof(gsz_str), "%d", group_size_arr[i]);
                char *args[] = {
                    "./utente",
                    idx_str,
                    (char *)config_file,
                    (char *)menu_file,
                    "0",  // barriera iniziale: da implementare
                    (i >= (n_utenti * 8 / 10)) ? "1" : "0",
                    gid_str,
                    gsz_str,
                    NULL
                };
                execve("./utente", args, environ);
                perror("[UTENTE] Errore execve");
                _exit(EXIT_FAILURE);
            }
            default:
                g_pids_utenti[i] = pid;
                break;
        }
    }
    return 0;
}

// CREA OPERATORI
static int crea_operatori_exec(int n_operatori, const char *config_file, const char *menu_file, const Config *cfg) {
    int i, j, k, tmp;
    pid_t pid;

    const char *nomi[4]  = {"PRIMI", "SECONDI", "COFFEE", "CASSA"};
    int         tempi[4] = {
        cfg->avg_srvc_primi,
        cfg->avg_srvc_secondi,
        cfg->avg_srvc_coffee,
        cfg->avg_srvc_cassa
    };

    // ordinazione delle stazioni per tempo di servizio decrescente
    int ordine[4] = {0, 1, 2, 3};
    for (j = 0; j < 3; j++) {
        int max = j;
        for (k = j + 1; k < 4; k++)
            if (tempi[ordine[k]] > tempi[ordine[max]])
                max = k;
        tmp = ordine[j]; ordine[j] = ordine[max]; ordine[max] = tmp;
    }

    for (i = 0; i < n_operatori; i++) {
        const char *stazione;

        if (i < 4)
            stazione = nomi[i];
        else
            stazione = nomi[ordine[(i - 4) % 4]]; // ordine x assegnare la stazione + lenta

        // Stesso pattern fork()+execve() usato per gli utenti
        switch (pid = fork()) {
            case -1:
                perror("[RESPONSABILE MENSA] Errore fork operatore");
                return -1;
            case 0: {
                char *args[] = {
                    "./operatore",
                    (char *)stazione,
                    (char *)config_file,
                    (char *)menu_file,
                    NULL
                };
                execve("./operatore", args, NULL);
                perror("[OPERATORE] Errore execve");
                _exit(EXIT_FAILURE);
            }
            default:
                g_pids_operatori[i] = pid;
                break;
        }
    }
    return 0;
}

// CREA RIFORNIMENTO
static int crea_rifornimento_exec(const char *config_file, const char *menu_file) {
    pid_t pid;
    switch (pid = fork()) {
        case -1:
            perror("[RESPONSABILE MENSA] Errore fork rifornimento");
            return -1;
        case 0: {
            char *args[] = {
                "./rifornimento",
                (char *)config_file,
                (char *)menu_file,
                NULL
            };
            execve("./rifornimento", args, NULL);
            perror("[RIFORNIMENTO] Errore execve");
            _exit(EXIT_FAILURE);
        }
        default:
            break;
    }
    return 0;
}

// ASPETTA FIGLI
static void aspetta_figli(void) {
    int status;
    pid_t pid;
    while (1) {
        pid = wait(&status);
        if (pid == -1) {
            if (errno == ECHILD) break;
            if (errno == EINTR)  continue;
            perror("[RESPONSABILE MENSA] Errore wait");
            break;
        }
    }
}

// SEGNALA FINE SIMULAZIONE
static void segnala_fine_simulazione(int sem_id, Stats *stats, int causa) {
    struct sembuf sops;
    sops.sem_num = SEM_MUTEX_STATS;
    sops.sem_flg = 0;
    sops.sem_op  = -1;
    if (semop(sem_id, &sops, 1) == -1) {
        perror("[RESPONSABILE MENSA] Errore semop -1 fine simulazione");
        return;
    }
    stats->simulazione_terminata = 1;
    if (stats->causa_terminazione == TERM_NONE)
        stats->causa_terminazione = causa;
    sops.sem_op = 1;  // esco dalla sezione critica
    semop(sem_id, &sops, 1);
}

// SBLOCCA UTENTI NON NECESSARI
static void sblocca_gruppi_incompleti(int sem_id, Stats *stats) {
    int g, k, n;
    struct sembuf s;

    s.sem_num = SEM_MUTEX_STATS; s.sem_flg = 0;
    s.sem_op  = -1; semop(sem_id, &s, 1);
    n = stats->n_gruppi;
    s.sem_op  =  1; semop(sem_id, &s, 1);

    for (g = 0; g < n; g++) {
        int waiting = 0, size;

        s.sem_num = SEM_MUTEX_STATS; s.sem_flg = 0;
        s.sem_op  = -1; semop(sem_id, &s, 1);
        size = stats->gruppi_size[g];
        if (size > 1)
            waiting = stats->gruppi_pronti[g] % size; // quanti utenti sono in attesa
        s.sem_op = 1; semop(sem_id, &s, 1);

        if (waiting > 0) {
            struct sembuf v;
            v.sem_num = (unsigned short)(SEM_GRUPPO_BASE + g);
            v.sem_flg = 0;
            v.sem_op  = 1;
            for (k = 0; k < waiting; k++)
                semop(sem_id, &v, 1);
            printf("[RESPONSABILE MENSA] Gruppo %d: sbloccati %d utenti in attesa\n",
                   g, waiting);
        }
    }
}

// CLEANUP IPC
static void cleanup_ipc(Stats *stats, int shm_id, int sem_id, int msg_id) {
    if (stats != NULL)                 shmdt(stats);
    if (shm_id != -1 && sem_id != -1) destroy_ipc(shm_id, sem_id);
    if (msg_id != -1)                  msgctl(msg_id, IPC_RMID, NULL);
}

// AVVIA RESPONSABILE MENSA
int avvia_responsabile_mensa(const char *config_file, const char *menu_file) {
    Config           cfg;
    int              shm_id, sem_id, msg_id;
    int              giorno, i;
    Stats           *stats;
    union semun      arg;
    struct message_t msg;

    if (load_config(config_file, &cfg) != 0) {
        fprintf(stderr, "[RESPONSABILE MENSA] Errore caricamento configurazione\n");
        return -1;
    }
    g_nof_workers = cfg.nof_workers;

    // registrazione dell'handler per terminazione con SIGINT e SIGTERM
    {
        struct sigaction sa;
        memset(&sa, 0, sizeof(sa));
        sa.sa_handler = handle_signal;
        if (sigaction(SIGINT,  &sa, NULL) == -1) {
            perror("[RESPONSABILE MENSA] Errore sigaction SIGINT");
            return -1;
        }
        if (sigaction(SIGTERM, &sa, NULL) == -1) {
            perror("[RESPONSABILE MENSA] Errore sigaction SIGTERM");
            return -1;
        }
        // SIGUSR2 ignorato dal responsabile: lo mandiamo noi ai figli, non vogliamo riceverlo
        sa.sa_handler = SIG_IGN;
        if (sigaction(SIGUSR2, &sa, NULL) == -1) {
            perror("[RESPONSABILE MENSA] Errore sigaction SIGUSR2");
            return -1;
        }
    }

    // inizializzazione delle risorse IPC
    if (init_ipc(&shm_id, &sem_id) != 0) {
        fprintf(stderr, "[RESPONSABILE MENSA] Errore inizializzazione IPC\n");
        return -1;
    }
    g_shm_id = shm_id;
    g_sem_id = sem_id;

    msg_id = msgget(MSG_KEY, 0);
    if (msg_id == -1) {
        perror("[RESPONSABILE MENSA] Errore msgget");
        destroy_ipc(shm_id, sem_id);
        return -1;
    }
    g_msg_id = msg_id;

    // Aggancio shared memory e inizializzazione statistiche
    stats = shmat(shm_id, NULL, 0);
    if (stats == (void *)-1) {
        perror("[RESPONSABILE MENSA] Errore shmat");
        destroy_ipc(shm_id, sem_id);
        msgctl(msg_id, IPC_RMID, NULL);
        return -1;
    }
    g_stats = stats;
    init_stats(stats);

    // inizializzazione porzioni
    for (i = 0; i < MAX_MENU_ITEMS; i++) {
        stats->porzioni_primi[i]   = cfg.avg_refill_primi;
        stats->porzioni_secondi[i] = cfg.avg_refill_secondi;
    }

    // Inizializzazione semafori
    arg.val = 1;
    if (semctl(sem_id, SEM_MUTEX_STATS, SETVAL, arg) == -1) {
        perror("[RESPONSABILE MENSA] Errore SETVAL SEM_MUTEX_STATS");
        cleanup_ipc(stats, shm_id, sem_id, msg_id);
        return -1;
    }

    // inizializzazione semafori per stazioni e tavoli
    arg.val = cfg.nof_wk_seats_primi;
    if (semctl(sem_id, SEM_STAZIONE_PRIMI, SETVAL, arg) == -1) {
        perror("[RESPONSABILE MENSA] Errore SETVAL SEM_STAZIONE_PRIMI");
        cleanup_ipc(stats, shm_id, sem_id, msg_id);
        return -1;
    }
    arg.val = cfg.nof_wk_seats_secondi;
    if (semctl(sem_id, SEM_STAZIONE_SECONDI, SETVAL, arg) == -1) {
        perror("[RESPONSABILE MENSA] Errore SETVAL SEM_STAZIONE_SECONDI");
        cleanup_ipc(stats, shm_id, sem_id, msg_id);
        return -1;
    }
    arg.val = cfg.nof_wk_seats_coffee;
    if (semctl(sem_id, SEM_STAZIONE_COFFEE, SETVAL, arg) == -1) {
        perror("[RESPONSABILE MENSA] Errore SETVAL SEM_STAZIONE_COFFEE");
        cleanup_ipc(stats, shm_id, sem_id, msg_id);
        return -1;
    }
    arg.val = cfg.nof_wk_seats_cassa;
    if (semctl(sem_id, SEM_STAZIONE_CASSA, SETVAL, arg) == -1) {
        perror("[RESPONSABILE MENSA] Errore SETVAL SEM_STAZIONE_CASSA");
        cleanup_ipc(stats, shm_id, sem_id, msg_id);
        return -1;
    }
    arg.val = cfg.nof_table_seats;
    if (semctl(sem_id, SEM_TAVOLI, SETVAL, arg) == -1) {
        perror("[RESPONSABILE MENSA] Errore SETVAL SEM_TAVOLI");
        cleanup_ipc(stats, shm_id, sem_id, msg_id);
        return -1;
    }

    /* Barriera iniziale: vale nof_users + nof_workers + 1 (rifornimento).
       Ogni processo decrementa quando ha finito l'init.
       Il responsabile aspetta che arrivi a 0 prima di avviare la simulazione. */
    arg.val = cfg.nof_users + cfg.nof_workers + 1;
    if (semctl(sem_id, SEM_MUTEX_SYNC, SETVAL, arg) == -1) {
        perror("[RESPONSABILE MENSA] Errore SETVAL SEM_MUTEX_SYNC");
        cleanup_ipc(stats, shm_id, sem_id, msg_id);
        return -1;
    }

    // SEM_FINE_SIM: gli operatori a fine simulazione si bloccano qui
    arg.val = 0;
    if (semctl(sem_id, SEM_FINE_SIM, SETVAL, arg) == -1) {
        perror("[RESPONSABILE MENSA] Errore SETVAL SEM_FINE_SIM");
        cleanup_ipc(stats, shm_id, sem_id, msg_id);
        return -1;
    }

    // SEM_TICKET: può accedere 1 utente alla volta al bancone del ticket
    arg.val = 1;
    if (semctl(sem_id, SEM_TICKET, SETVAL, arg) == -1) {
        perror("[RESPONSABILE MENSA] Errore SETVAL SEM_TICKET");
        cleanup_ipc(stats, shm_id, sem_id, msg_id);
        return -1;
    }

    // segnali di sincronizzazione: SEM_FINE_GIORNATA e SEM_INIZIO_GIORNATA, inizializ. a 0
    arg.val = 0;
    if (semctl(sem_id, SEM_FINE_GIORNATA, SETVAL, arg) == -1) {
        perror("[RESPONSABILE MENSA] Errore SETVAL SEM_FINE_GIORNATA");
        cleanup_ipc(stats, shm_id, sem_id, msg_id);
        return -1;
    }
    arg.val = 0;
    if (semctl(sem_id, SEM_INIZIO_GIORNATA, SETVAL, arg) == -1) {
        perror("[RESPONSABILE MENSA] Errore SETVAL SEM_INIZIO_GIORNATA");
        cleanup_ipc(stats, shm_id, sem_id, msg_id);
        return -1;
    }

    // Lancio operatori
    if (crea_operatori_exec(cfg.nof_workers, config_file, menu_file, &cfg) != 0) {
        fprintf(stderr, "[RESPONSABILE MENSA] Errore creazione operatori\n");
        cleanup_ipc(stats, shm_id, sem_id, msg_id);
        return -1;
    }

    // attendo operatori pronti
    for (i = 0; i < cfg.nof_workers; i++) {
        if (msgrcv(msg_id, &msg, MSG_LEN, MSG_TYPE_OPERATOR_READY, 0) == -1) {
            perror("[RESPONSABILE MENSA] Errore msgrcv");
            cleanup_ipc(stats, shm_id, sem_id, msg_id);
            return -1;
        }
    }

    // Lancio utenti
    srand((unsigned int)getpid()); // inizial. generatore con il PID
    if (crea_utenti_exec(cfg.nof_users, config_file, menu_file,
                         cfg.max_users_per_group > 0 ? cfg.max_users_per_group : 1, // se il val nel config fosse 0 uso 1 come val minimo
                         stats) != 0) {
        fprintf(stderr, "[RESPONSABILE MENSA] Errore creazione utenti\n");
        cleanup_ipc(stats, shm_id, sem_id, msg_id);
        return -1;
    }

    // Lancio rifornimento
    if (crea_rifornimento_exec(config_file, menu_file) != 0) {
        fprintf(stderr, "[RESPONSABILE MENSA] Errore creazione rifornimento\n");
        cleanup_ipc(stats, shm_id, sem_id, msg_id);
        return -1;
    }

    // Sincronizzazione iniziale: attende che tutti i processi abbiano finito l'init
    {
        struct sembuf sops;
        sops.sem_num = SEM_MUTEX_SYNC;
        sops.sem_flg = 0;
        sops.sem_op  = 0;
        if (semop(sem_id, &sops, 1) == -1) {
            perror("[RESPONSABILE MENSA] Errore attesa barriera iniziale");
            cleanup_ipc(stats, shm_id, sem_id, msg_id);
            return -1;
        }
    }

    printf("[RESPONSABILE MENSA] Simulazione avviata\n");

    // Loop giornate
    for (giorno = 1; giorno <= cfg.sim_duration; giorno++) {
        struct sembuf s;
        union semun   r;
        int k;

        // Controllo overload: se qualcuno ha già impostato terminata, usciamo
        s.sem_num = SEM_MUTEX_STATS; s.sem_flg = 0;
        s.sem_op  = -1; semop(sem_id, &s, 1);
        {
            int terminata = stats->simulazione_terminata;
            s.sem_op = 1; semop(sem_id, &s, 1);
            if (terminata) break;
        }

        // Aggiorno numero giorno e salvo snapshot statistiche per calcoli fine giornata
        s.sem_num = SEM_MUTEX_STATS; s.sem_flg = 0;
        s.sem_op  = -1; semop(sem_id, &s, 1);
        scatta_snapshot(stats);  // da implementare
        stats->num_giorno    = giorno;
        //reset flag di fine giornata per il nuovo giorno
        stats->fine_giornata = 0;
        s.sem_op =  1; semop(sem_id, &s, 1);

        // Sblocco utenti a inizio giornata
        {
            int u; struct sembuf sv;
            sv.sem_num = SEM_INIZIO_GIORNATA; sv.sem_op = 1; sv.sem_flg = 0;
            for (u = 0; u < (int)cfg.nof_users; u++)
                semop(sem_id, &sv, 1);
        }

        struct timespec ts;
        long ns_totali = (long)cfg.minuti_giornata * cfg.n_nano_secs;
        ts.tv_sec  = ns_totali / 1000000000L;
        ts.tv_nsec = ns_totali % 1000000000L;
        nanosleep(&ts, NULL);

        // Setto fine_giornata e mando SIGUSR2 ai figli per svegliarli da chiamate bloccanti
        s.sem_num = SEM_MUTEX_STATS; s.sem_flg = 0;
        s.sem_op  = -1; semop(sem_id, &s, 1);
        stats->fine_giornata = 1;
        s.sem_op  =  1; semop(sem_id, &s, 1);
        for (k = 0; k < g_n_utenti;    k++) kill(g_pids_utenti[k],    SIGUSR2);
        for (k = 0; k < g_nof_workers; k++) kill(g_pids_operatori[k], SIGUSR2);

        // fine giornata: sblocco utenti bloccati alla barriera di gruppo
        sblocca_gruppi_incompleti(sem_id, stats);

        // Aspetta che tutti gli utenti abbiano finito la giornata
        s.sem_num = SEM_FINE_GIORNATA;
        s.sem_op  = -(int)cfg.nof_users;
        s.sem_flg = 0;
        semop(sem_id, &s, 1);

        // Reset SEM_FINE_GIORNATA per il giorno successivo
        r.val = 0;
        semctl(sem_id, SEM_FINE_GIORNATA, SETVAL, r);

        printf("\n========== STATISTICHE FINE GIORNATA %d ==========\n", giorno);
        print_stats_giornata(stats, giorno);  // da implementare
        print_stats(stats);

    }  // fine loop giornate

    /* Fine simulazione: segnala causa TERM_TIMEOUT nella shm.
       Se era già stata impostata (overload), non la sovrascrive. */
    segnala_fine_simulazione(sem_id, stats, TERM_TIMEOUT);

    // Sblocca utenti rimasti in attesa della barriera pre-cassa
    sblocca_gruppi_incompleti(sem_id, stats);

    // Sblocca operatori bloccati su SEM_FINE_SIM
    {
        struct sembuf v;
        int j;
        v.sem_num = SEM_FINE_SIM;
        v.sem_flg = 0;
        v.sem_op  = 1;
        for (j = 0; j < cfg.nof_workers; j++)
            semop(sem_id, &v, 1);
    }

    // Aspetta terminazione di tutti i processi figli
    aspetta_figli();

    // Conta porzioni avanzate a fine simulazione
    {
        int j;
        struct sembuf s;
        s.sem_num = SEM_MUTEX_STATS; s.sem_flg = 0;
        s.sem_op  = -1; semop(sem_id, &s, 1);
        for (j = 0; j < MAX_MENU_ITEMS; j++) {
            stats->avanzi_primi_totali   += stats->porzioni_primi[j];
            stats->avanzi_secondi_totali += stats->porzioni_secondi[j];
        }
        s.sem_op = 1; semop(sem_id, &s, 1);
    }

    // Stampa report finale e salva statistiche su CSV
    print_stats(stats);
    salva_stats_csv(stats, "statistiche.csv");

    // Pulizia risorse IPC
    if (shmdt(stats) == -1)
        perror("[RESPONSABILE MENSA] Errore shmdt");
    g_stats = NULL;

    destroy_ipc(shm_id, sem_id);
    g_shm_id = -1;
    g_sem_id = -1;

    if (msgctl(msg_id, IPC_RMID, NULL) == -1)
        perror("[RESPONSABILE MENSA] Errore msgctl IPC_RMID");
    g_msg_id = -1;

    printf("[RESPONSABILE MENSA] Fine simulazione\n");
    return 0;
}
