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
    int i, g;
    pid_t pid;

    int group_id[1024];
    int group_size_arr[1024];
    if (n_utenti > 1024) {
        fprintf(stderr, "[RESPONSABILE MENSA] Troppi utenti (max 1024)\n");
        return -1;
    }


    // assegno i gruppi a tutti gli utenti prima di fare fork()
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

        printf("[RESPONSABILE MENSA] Gruppo %d: utenti %d..%d (size=%d)\n",
               g, i, i + size - 1, size);

        i += size;
        g++;
    }
    stats->n_gruppi = g;
    g_n_gruppi      = g;  


     // fork()+execve() per ogni utente
    for (i = 0; i < n_utenti; i++) {
        switch (pid = fork()) {

            case -1:
                perror("[RESPONSABILE MENSA] Errore fork utente");
                return -1;

            case 0: {
                
                char idx_str[16], gid_str[16], gsz_str[16];
                char *args[] = {
                    "./utente",
                    idx_str,
                    (char *)config_file,
                    (char *)menu_file,
                    "0",                                    // barriera iniziale
                    (i >= (n_utenti * 8 / 10)) ? "1" : "0"

                    gid_str,
                    gsz_str,
                    NULL
                };
                snprintf(idx_str, sizeof(idx_str), "%d", i);
                snprintf(gid_str, sizeof(gid_str), "%d", group_id[i]);
                snprintf(gsz_str, sizeof(gsz_str), "%d", group_size_arr[i]);

                execve("./utente", args, NULL);
                perror("[UTENTE] Errore execve");
                _exit(EXIT_FAILURE);
            }

            default:
                printf("[RESPONSABILE MENSA] Creato utente %d (gruppo=%d, size=%d), PID=%d\n",
                       i, group_id[i], group_size_arr[i], pid);
                break;
        }
    }
    return 0;
}
