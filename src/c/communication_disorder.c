
 // communication_disorder.c
 
#include "common.h"
#include "config.h"
#include "ipc.h"
#include "stats.h"

int main(int argc, char *argv[]) {
    Config  cfg;
    Stats  *stats;
    int     shm_id, sem_id, i;
    struct sembuf sops;

    if (argc < 3) {
        fprintf(stderr, "Uso: %s <config_file> <menu_file>\n", argv[0]);
        return 1;
    }

    if (load_config(argv[1], &cfg) != 0) {
        fprintf(stderr, "[COMM_DISORDER] Errore caricamento config\n");
        return 1;
    }

    // Aggancio shared memory e semafori già creati dal responsabile 
    shm_id = shmget(SHM_KEY, sizeof(Stats), 0);
    if (shm_id == -1) { perror("[COMM_DISORDER] shmget"); return 1; }

    stats = shmat(shm_id, NULL, 0);
    if (stats == (void *)-1) { perror("[COMM_DISORDER] shmat"); return 1; }

    sem_id = semget(SEM_KEY, NUM_SEMS, 0);
    if (sem_id == -1) { perror("[COMM_DISORDER] semget"); shmdt(stats); return 1; }

    // Aspetta che la simulazione sia partita 
    sops.sem_num = SEM_MUTEX_SYNC;
    sops.sem_flg = 0;
    sops.sem_op  = 0;
    if (semop(sem_id, &sops, 1) == -1) {
        perror("[COMM_DISORDER] semop sync");
        shmdt(stats);
        return 1;
    }

    // Colpisce in un momento casuale della simulazione 
    srand((unsigned int)getpid());
    sleep(1 + rand() % (cfg.sim_duration / 2 + 1));

    // Controlla se la simulazione è già terminata 
    sem_p(sem_id, SEM_MUTEX_STATS);
    i = stats->simulazione_terminata;
    sem_v(sem_id, SEM_MUTEX_STATS);
    if (i) { shmdt(stats); return 0; }

    // Blocc la cassa occupando tutti i posti 
    printf("[COMM_DISORDER] Blocco cassa per %d secondi\n", cfg.stop_duration);
    for (i = 0; i < cfg.nof_wk_seats_cassa; i++)
        sem_p(sem_id, SEM_STAZIONE_CASSA);

    sleep(cfg.stop_duration);

    // Rilascio la cassa 
    for (i = 0; i < cfg.nof_wk_seats_cassa; i++)
        sem_v(sem_id, SEM_STAZIONE_CASSA);
    printf("[COMM_DISORDER] Cassa sbloccata\n");

    shmdt(stats);
    return 0;
}