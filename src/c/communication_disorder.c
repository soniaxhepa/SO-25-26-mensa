// communication_disorder.c

#include "../../include/ipc.h"
#include "../../include/stats.h"
#include "../../include/config.h"

int main(int argc, char *argv[]) {
    Config cfg;
    Stats *stats;
    int shm_id, sem_id;
    struct sembuf sops;

    if (argc < 2) {
        fprintf(stderr, "Uso: %s <config_file>\n", argv[0]);
        return 1;
    }
    if (load_config(argv[1], &cfg) != 0) {
        fprintf(stderr, "[COMM_DISORDER] Errore caricamento config\n");
        return 1;
    }

    // Aggancio shared memory e semafori già creati dal responsabile
    shm_id = shmget(SHM_KEY, sizeof(Stats), 0);
    if (shm_id == -1) { perror("[COMM_DISORDER] shmget"); return 1; }

    stats = (Stats *)shmat(shm_id, NULL, 0);
    if (stats == (Stats *)-1) { perror("[COMM_DISORDER] shmat"); return 1; }

    sem_id = semget(SEM_KEY, 0, 0);
    if (sem_id == -1) { perror("[COMM_DISORDER] semget"); shmdt(stats); return 1; }

    // Aspetta che la simulazione sia partita (SEM_MUTEX_SYNC va a 0 quando tutti pronti)
    sops.sem_num = SEM_MUTEX_SYNC;
    sops.sem_flg = 0;
    sops.sem_op  = 0;
    if (semop(sem_id, &sops, 1) == -1) {
        perror("[COMM_DISORDER] semop sync");
        shmdt(stats);
        return 1;
    }

    // Colpisce in un momento casuale della simulazione (tempo simulato)
    srand((unsigned int)getpid());
    long minuti_attesa = 1 + rand() % (cfg.minuti_giornata / 2 + 1);
    struct timespec ts;
    ts.tv_sec  = (minuti_attesa * cfg.n_nano_secs) / 1000000000L;
    ts.tv_nsec = (minuti_attesa * cfg.n_nano_secs) % 1000000000L;
    nanosleep(&ts, NULL);

    // Controlla se la simulazione è già terminata
    sem_p(sem_id, SEM_MUTEX_STATS);
    int terminata = stats->simulazione_terminata;
    sem_v(sem_id, SEM_MUTEX_STATS);
    if (terminata) { shmdt(stats); return 0; }

    // Blocco la cassa settando il flag in shared memory
    printf("[COMM_DISORDER] Blocco cassa per %d minuti simulati\n", cfg.stop_duration);
    sem_p(sem_id, SEM_MUTEX_STATS);
    stats->cassa_bloccata = 1;
    sem_v(sem_id, SEM_MUTEX_STATS);

    // Attendo stop_duration (tempo simulato)
    ts.tv_sec  = ((long)cfg.stop_duration * cfg.n_nano_secs) / 1000000000L;
    ts.tv_nsec = ((long)cfg.stop_duration * cfg.n_nano_secs) % 1000000000L;
    nanosleep(&ts, NULL);

    // Rilascio la cassa
    sem_p(sem_id, SEM_MUTEX_STATS);
    stats->cassa_bloccata = 0;
    sem_v(sem_id, SEM_MUTEX_STATS);
    printf("[COMM_DISORDER] Cassa sbloccata\n");

    shmdt(stats);
    return 0;
}