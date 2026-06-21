#include "../../include/ipc.h"
#include "../../include/stats.h"
#include "../../include/config.h"

// firma del main pk rifornimento è un eseguibile separato lanciato con execve
int main(int argc, char *argv[]) {

    // controllo argc
    if (argc < 2) {
        fprintf(stderr, "Uso: %s <config>\n", argv[0]);
        return 1;
    }

    // carica config 
    Config cfg;
    if (load_config(argv[1], &cfg) != 0) {
        fprintf(stderr, "[RIFORNIMENTO] Errore caricamento config\n");
        return 1;
    }
    
    // aggancia IPC
    int shm_id = shmget(SHM_KEY, sizeof(Stats), 0);
    if (shm_id == -1) { perror("[RIFORNIMENTO] shmget"); return 1; }

    int sem_id = semget(SEM_KEY, 0, 0);
    if (sem_id == -1) { perror("[RIFORNIMENTO] semget"); return 1; }

    Stats *stats = (Stats *) shmat(shm_id, NULL, 0);
    if (stats == (Stats *)-1) { perror("[RIFORNIMENTO] shmat"); return 1; }

    srand((unsigned int)getpid());
    { struct sembuf s; s.sem_num = SEM_MUTEX_SYNC; s.sem_flg = 0;
      s.sem_op = -1; semop(sem_id, &s, 1);
      s.sem_op =  0; semop(sem_id, &s, 1); }

    // loop rifornimento (Questo processo deve:
    //      Aspettare avg_refill_time secondi (simulati)
    //      Ricaricare le porzioni di primi e secondi fino al massimo
    //      Ripetere finché la simulazione non è terminata)
    while (!stats->simulazione_terminata) {
    int var = cfg.avg_refill_time * 20 / 100;
    int effettivo = cfg.avg_refill_time - var + rand() % (2 * var + 1);
    struct timespec ts;
    ts.tv_sec  = ((long)effettivo * cfg.n_nano_secs) / 1000000000L;
    ts.tv_nsec = ((long)effettivo * cfg.n_nano_secs) % 1000000000L;
    
    //nanosleep(&ts, NULL); anzichè questo 
    if (nanosleep(&ts, NULL) == -1 && errno == EINTR) {
        if (stats->simulazione_terminata) break;
        continue;
    }
    
    if (stats->simulazione_terminata) break;
    
    sem_p(sem_id, SEM_MUTEX_STATS);
    
    // ricarica primi
    int i;
    for (i = 0; i < MAX_MENU_ITEMS; i++) {
        if (stats->porzioni_primi[i] < cfg.max_porzioni_primi)
            stats->porzioni_primi[i] = cfg.max_porzioni_primi;
        }
    // ricarica secondi
    for (i = 0; i < MAX_MENU_ITEMS; i++) {
        if (stats->porzioni_secondi[i] < cfg.max_porzioni_secondi)
            stats->porzioni_secondi[i] = cfg.max_porzioni_secondi;
        }
    
    sem_v(sem_id, SEM_MUTEX_STATS);
    printf("[RIFORNIMENTO] Porzioni ricaricate\n");
    }

    shmdt(stats);
    return 0;

}
