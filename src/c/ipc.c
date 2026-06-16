#include "../../include/ipc.h"
#include "../../include/stats.h"

int init_ipc(int *shm_id, int *sem_id) {
    // creo per prima cosa la shared memory xkè shmget deve sapere
    // quanto spazio allocare (la dim è sizeof(Stats))


    //E' un flag, una costante che si combina con gli OR e i permessi (0666)
    *shm_id = shmget(SHM_KEY, sizeof(Stats), IPC_CREAT | 0666); //0666->  tutti possono leggere e scrivere
    
    //controllo se shmget è fallito
    if (*shm_id == -1) {
        perror("[IPC] Error shmget");
        return -1;
    }

    //si passa alla creazione dei semafori
    *sem_id = semget(SEM_KEY, NUM_SEMS, IPC_CREAT | 0666); //semget crea un set di semafori, non uno alla volta
    if (*sem_id == -1) {
        perror("[IPC] Errore semget");
        return -1;
    }

    //infine creo coda messaggi
    //unica dif rispetto agli altri: msg_id non lo passo come puntatore pk init_ipc non lo restituisce al chiamante,
    //ma viene trovata dai processi con msgget(MSG_KEY, 0).
    int msg_id = msgget(MSG_KEY, IPC_CREAT | 0666);
    if (msg_id == -1) {
        perror("[IPC] Errore msgget");
        return -1;
    }

    return 0;
}
    
int destroy_ipc(int shm_id, int sem_id) {
    // rimuove shared memory
    shmctl(shm_id, IPC_RMID, NULL);
    // rimuove set semafori
    semctl(sem_id, 0, IPC_RMID);
        
    // Non ho l'id dei messaggi di coda, lo recupero senza IPC_CREAT
    int msg_id = msgget(MSG_KEY, 0);
    if (msg_id != -1)
        //rimuove coda di mes
        msgctl(msg_id, IPC_RMID, NULL);

    return 0;
}

void sem_p(int sem_id, int sem_num) {
    struct sembuf sb;
    sb.sem_num = sem_num;       // quale semaforo nel set
    sb.sem_op  = -1;            // P = decrementa (aspetta se è 0)
    sb.sem_flg = 0;
    semop(sem_id, &sb, 1);      // dice quante operazioni svoglgere per volta
}

void sem_v(int sem_id, int sem_num) {
    struct sembuf sb;
    sb.sem_num = sem_num;
    sb.sem_op  = +1;            // V = incrementa (sblocca chi aspetta)
    sb.sem_flg = 0;
    semop(sem_id, &sb, 1);
}
