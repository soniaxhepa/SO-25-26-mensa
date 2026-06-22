// AGGIUNGI UTENTI
#include "../../include/ipc.h"    
#include "../../include/stats.h"  
#include "../../include/config.h" 
int main(int argc, char *argv[])
{
    Config  cfg;
    int     shm_id, sem_id;
    Stats  *stats;
    int     terminata;
    int     i;
    struct sembuf s;
    if (argc < 3) {
        fprintf(stderr,
            "Uso: %s <config_file> <menu_file>\n"
            "Esempio: ./aggiungi_utenti config/config_timeout.conf config/menu.txt\n",
            argv[0]);
        return 1;
    }
    //carico la configurazione
    if (load_config(argv[1], &cfg) != 0) {
        fprintf(stderr, "[AGGIUNGI_UTENTI] Errore caricamento configurazione\n");
        return 1;
    }
    if (cfg.n_new_users <= 0) {
        fprintf(stderr,
            "[AGGIUNGI_UTENTI] N_NEW_USERS non configurato o zero, niente da fare.\n");
        return 0;
    }
    //aggancio IPC
    shm_id = shmget(SHM_KEY, sizeof(Stats), 0);
    if (shm_id == -1) {
        perror("[AGGIUNGI_UTENTI] shmget — la simulazione è in corso?");
        return 1;
    }
    stats = shmat(shm_id, NULL, 0);
    if (stats == (void *)-1) {
        perror("[AGGIUNGI_UTENTI] shmat");
        return 1;
    }
    sem_id = semget(SEM_KEY, 0, 0);
    if (sem_id == -1) {
        perror("[AGGIUNGI_UTENTI] semget");
        shmdt(stats);
        return 1;
    }
    //Controllo che la simulazione non sia terminata
    s.sem_num = SEM_MUTEX_STATS; s.sem_flg = 0;
    s.sem_op = -1; semop(sem_id, &s, 1);
    terminata = stats->simulazione_terminata;
    s.sem_op =  1; semop(sem_id, &s, 1);
    if (terminata) {
        fprintf(stderr,
            "[AGGIUNGI_UTENTI] Simulazione già terminata, impossibile aggiungere utenti.\n");
        shmdt(stats);
        return 1;
    }
    // creo i nuovi utenti
    srand((unsigned int)getpid());
    for (i = 0; i < cfg.n_new_users; i++) {
        pid_t pid;
        char  idx_str[16];
        char  ticket_str[4];
        char *args[9];
        snprintf(idx_str, sizeof(idx_str), "%d", 10000 + i);
        snprintf(ticket_str, sizeof(ticket_str), "%d", (rand() % 10 < 8) ? 1 : 0);
        args[0] = "./utente";
        args[1] = idx_str;   
        args[2] = argv[1];   
        args[3] = argv[2];   
        args[4] = "1";       
        args[5] = ticket_str;
        args[6] = "-1";      
        args[7] = "1";       
        args[8] = NULL;
        switch (pid = fork()) {
            case -1:
                perror("[AGGIUNGI_UTENTI] Errore fork");
                shmdt(stats);
                return 1;
            case 0:
                setsid();   /* distacca dal terminale controllante */
                execve("./utente", args, NULL);
                perror("[AGGIUNGI_UTENTI] Errore execve");
                _exit(EXIT_FAILURE);
            default:
                break;
        }
    }
    //distacco memoria
    if (shmdt(stats) == -1)
        perror("[AGGIUNGI_UTENTI] shmdt");
    return 0;
}