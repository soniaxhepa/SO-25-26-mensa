#ifndef IPC_H
#define IPC_H

#include "common.h"

#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/msg.h>

#define MSG_LEN 120
#define MSG_TYPE_OPERATOR_READY 1

#define SHM_KEY 2401
#define SEM_KEY 1967
#define MSG_KEY 7706

#define SEM_MUTEX_STATS       0
#define SEM_MUTEX_SYNC        1
#define SEM_STAZIONE_PRIMI    2
#define SEM_STAZIONE_SECONDI  3
#define SEM_STAZIONE_COFFEE   4
#define SEM_STAZIONE_CASSA    5
#define SEM_TAVOLI            6

#define SEM_FINE_SIM          7
#define SEM_FINE_GIORNATA     8 
#define SEM_INIZIO_GIORNATA   9 
#define SEM_TICKET            10
#define SEM_GRUPPO_BASE       11
#define MAX_GRUPPI_SEM        32
#define NUM_SEMS              (SEM_GRUPPO_BASE + MAX_GRUPPI_SEM)

// cause terminazione
#define TERM_NONE     0
#define TERM_TIMEOUT  1
#define TERM_OVERLOAD 2

struct message_t {
    long mtype;
    char mtext[MSG_LEN];
};


int init_ipc(int *shm_id, int *sem_id);

int destroy_ipc(int shm_id, int sem_id);

void sem_p(int sem_id, int sem_num);
void sem_v(int sem_id, int sem_num);

typedef struct {
    pid_t pid;
    int   piatto;
    int   prezzo_cent;
} ReqPayload;

typedef struct {
    int servito;
    int prezzo_cent;
} ResPayload;

#endif
