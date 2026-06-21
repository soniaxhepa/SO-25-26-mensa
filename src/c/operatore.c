#include "../../include/ipc.h"
#include "../../include/stats.h"
#include "../../include/config.h"
    

//l'operatore riceve da execve la stazione a cui è assegnato e il config file
int main(int argc, char *argv[]) {
    if (argc < 4) {
        fprintf(stderr, "Uso: %s <stazione> <config> <menu>\n", argv[0]);
        return 1;
    }
    const char *stazione = argv[1];

    Config cfg;
    if (load_config(argv[2], &cfg) != 0) {
        fprintf(stderr, "[OPERATORE %s] Errore caricamento config\n", stazione);
        return 1;
    }

    srand((unsigned int)getpid());

    int shm_id = shmget(SHM_KEY, sizeof(Stats), 0);
    if (shm_id == -1) { perror("[OPERATORE] shmget"); return 1; }

    int sem_id = semget(SEM_KEY, 0, 0);
    if (sem_id == -1) { perror("[OPERATORE] semget"); return 1; }

    int msg_id = msgget(MSG_KEY, 0);
    if (msg_id == -1) { perror("[OPERATORE] msgget"); return 1; }

    Stats *stats = (Stats *) shmat(shm_id, NULL, 0);
    if (stats == (Stats *)-1) { perror("[OPERATORE] shmat"); return 1; }

    struct message_t msg;
    msg.mtype = MSG_TYPE_OPERATOR_READY;
    msgsnd(msg_id, &msg, 0, 0);

    {
    struct sembuf s;
    s.sem_num = SEM_MUTEX_SYNC; s.sem_flg = 0;
    s.sem_op  = -1;
    semop(sem_id, &s, 1);
    s.sem_op  =  0;
    semop(sem_id, &s, 1);
    }

    printf("[OPERATORE %s] passata barriera\n", stazione);

    if (strcmp(stazione, "PRIMI") == 0)        sem_p(sem_id, SEM_STAZIONE_PRIMI);
    else if (strcmp(stazione, "SECONDI") == 0) sem_p(sem_id, SEM_STAZIONE_SECONDI);
    else if (strcmp(stazione, "COFFEE") == 0)  sem_p(sem_id, SEM_STAZIONE_COFFEE);
    else                                        sem_p(sem_id, SEM_STAZIONE_CASSA);

    int staz_idx;
    if (strcmp(stazione, "PRIMI") == 0)        staz_idx = 0;
    else if (strcmp(stazione, "SECONDI") == 0) staz_idx = 1;
    else if (strcmp(stazione, "COFFEE") == 0)  staz_idx = 2;
    else                                        staz_idx = 3;

    int sem_staz;
    if (strcmp(stazione, "PRIMI") == 0)        sem_staz = SEM_STAZIONE_PRIMI;
    else if (strcmp(stazione, "SECONDI") == 0) sem_staz = SEM_STAZIONE_SECONDI;
    else if (strcmp(stazione, "COFFEE") == 0)  sem_staz = SEM_STAZIONE_COFFEE;
    else                                        sem_staz = SEM_STAZIONE_CASSA;

    /* registra operatore come attivo */
    sem_p(sem_id, SEM_MUTEX_STATS);
    stats->operatori_attivi_per_stazione[staz_idx]++;
    stats->operatori_attivi_totali++;
    sem_v(sem_id, SEM_MUTEX_STATS);

    while (!stats->simulazione_terminata){
        if (stats->fine_giornata) { 
            struct timespec ts = {0, 10000000L};
            nanosleep(&ts, NULL);
            continue;
        }   
        struct message_t msg;
        struct message_t risposta;
    
        long tipo_richiesta;
        if (strcmp(stazione, "PRIMI") == 0)        tipo_richiesta = MSG_REQ_PRIMI;
        else if (strcmp(stazione, "SECONDI") == 0) tipo_richiesta = MSG_REQ_SECONDI;
        else if (strcmp(stazione, "COFFEE") == 0)  tipo_richiesta = MSG_REQ_COFFEE;
        else                                       tipo_richiesta = MSG_REQ_CASSA;

        if (msgrcv(msg_id, &msg, MSG_LEN, tipo_richiesta, IPC_NOWAIT) == -1) {
        if (errno == ENOMSG) {
            struct timespec ts = {0, 10000000L};
            nanosleep(&ts, NULL);
            continue;
            }
        break;
        }
    
        int tempo_servizio;
        if (strcmp(stazione, "PRIMI") == 0)        tempo_servizio = cfg.avg_srvc_primi;
        else if (strcmp(stazione, "SECONDI") == 0) tempo_servizio = cfg.avg_srvc_secondi;
        else if (strcmp(stazione, "COFFEE") == 0)  tempo_servizio = cfg.avg_srvc_coffee;
        else                                       tempo_servizio = cfg.avg_srvc_cassa;

        int percentuale;
        if (strcmp(stazione, "COFFEE") == 0)      percentuale = 80;
        else if (strcmp(stazione, "CASSA") == 0)  percentuale = 20;
        else                                       percentuale = 50;
    
        int varianza = tempo_servizio * percentuale / 100;
        int effettivo = tempo_servizio - varianza + rand() % (2 * varianza + 1);
    
        struct timespec ts;
        ts.tv_sec  = (effettivo * cfg.n_nano_secs) / 1000000000;
        ts.tv_nsec = (effettivo * cfg.n_nano_secs) % 1000000000;
        nanosleep(&ts, NULL);
        
        ResPayload res;
        res.servito = 1;
            
        /* manda risposta all'utente */
        ReqPayload req;
        memcpy(&req, msg.mtext, sizeof(ReqPayload));
        risposta.mtype = (long)req.pid;
        memcpy(risposta.mtext, &res, sizeof(ResPayload));
        msgsnd(msg_id, &risposta, MSG_LEN, 0);
        
        /* logica pausa */
        int posso_pausare = 0;
        sem_p(sem_id, SEM_MUTEX_STATS);
        if (stats->operatori_attivi_per_stazione[staz_idx] > 1) {  
                stats->operatori_attivi_per_stazione[staz_idx]--;  
                posso_pausare = 1;
                }
        sem_v(sem_id, SEM_MUTEX_STATS);
        
        if (posso_pausare) {
            /* libera postazione */
                if (strcmp(stazione, "PRIMI") == 0)        sem_v(sem_id, SEM_STAZIONE_PRIMI);
                else if (strcmp(stazione, "SECONDI") == 0) sem_v(sem_id, SEM_STAZIONE_SECONDI);
                else if (strcmp(stazione, "COFFEE") == 0)  sem_v(sem_id, SEM_STAZIONE_COFFEE);
                else                                        sem_v(sem_id, SEM_STAZIONE_CASSA);
        
                /* pausa */
                struct timespec tp;
                tp.tv_sec  = ((long)cfg.nof_pause * cfg.n_nano_secs) / 1000000000L;
                tp.tv_nsec = ((long)cfg.nof_pause * cfg.n_nano_secs) % 1000000000L;
                nanosleep(&tp, NULL);
        
                /* riprende postazione */
                if (strcmp(stazione, "PRIMI") == 0)        sem_p(sem_id, SEM_STAZIONE_PRIMI);
                else if (strcmp(stazione, "SECONDI") == 0) sem_p(sem_id, SEM_STAZIONE_SECONDI);
                else if (strcmp(stazione, "COFFEE") == 0)  sem_p(sem_id, SEM_STAZIONE_COFFEE);
                else                                        sem_p(sem_id, SEM_STAZIONE_CASSA);
                
                printf("[OPERATORE %s] acquisita stazione\n", stazione);

                sem_p(sem_id, SEM_MUTEX_STATS);
                stats->operatori_attivi_per_stazione[staz_idx]++;  
                stats->pause_totali++;
                sem_v(sem_id, SEM_MUTEX_STATS);
        
                }
        
        }
    
    sem_v(sem_id, sem_staz);
    sem_p(sem_id, SEM_FINE_SIM);

    shmdt(stats);
    return 0;
                
}          
