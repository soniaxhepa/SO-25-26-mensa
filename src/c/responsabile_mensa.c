// Processo principale che avvia la simulazione

#include "responsabile_mensa.h"

static int    g_shm_id     = -1;
static int    g_sem_id     = -1;
static int    g_msg_id     = -1;
static Stats *g_stats      = NULL;
static int    g_nof_workers = 0;
static int    g_n_gruppi    = 0;

//handle signal, che gestisce i segnali SIGINT e SIGTERM
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

    //Pulizia finale
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
    int group_id[n_utenti];
    int group_size_arr[n_utenti];

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
                    "0",  // barriera iniziale
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
                break;
        }
    }
    return 0;
}

// CREA OPERATORI
tatic int crea_operatori_exec(int n_operatori, const char *config_file, const char *menu_file, const Config *cfg){
    int i, j, k, tmp;
    pid_t pid;

    const char *nomi[4]  = {"PRIMI", "SECONDI", "COFFEE", "CASSA"};
    int         tempi[4] = {
        cfg->avg_srvc_primi,
        cfg->avg_srvc_secondi,
        cfg->avg_srvc_coffee,
        cfg->avg_srvc_cassa
    };

    // ordinazione delle stazioni
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
            stazione = nomi[ordine[(i - 4) % 4]]; //ordine x assegnare la stazione + lenta

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
    int   status;
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

    sops.sem_op = -1;  
    if (semop(sem_id, &sops, 1) == -1) {
        perror("[RESPONSABILE MENSA] Errore semop -1 fine simulazione");
        return;
    }
    stats->simulazione_terminata = 1;
    if (stats->causa_terminazione == TERM_NONE)
        stats->causa_terminazione = causa;
    sops.sem_op = 1;   //esco dalla sezione critica
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
            waiting = stats->gruppi_pronti[g] % size; //quanti gruppi sono in attesa
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

// CLEAN_UP
static void cleanup_ipc(Stats *stats, int shm_id, int sem_id, int msg_id) {
    if (stats != NULL)  shmdt(stats);
    if (shm_id != -1 && sem_id != -1) destroy_ipc(shm_id, sem_id);
    if (msg_id != -1)   msgctl(msg_id, IPC_RMID, NULL);
}

