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
    printf("\n[RESPONSABILE MENSA] Segnale ricevuto, pulizia IPC...\n");

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
    // Rimozione segmento shm(mem. cond) e set semafori
    if (g_shm_id != -1 && g_sem_id != -1)
        destroy_ipc(g_shm_id, g_sem_id);
    // Rimozione coda messaggi
    if (g_msg_id != -1)
        msgctl(g_msg_id, IPC_RMID, NULL);

    exit(EXIT_SUCCESS);
}