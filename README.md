# Progetto Sistemi Operativi 2025/26

## Servizio Mensa “Oasi del Golfo”

Progetto realizzato per il corso di Sistemi Operativi. Il programma simula il funzionamento di una mensa tramite più processi concorrenti che comunicano usando meccanismi IPC System V.

## Componenti del progetto

La simulazione è composta da più eseguibili separati:

- `mensa`: processo principale che avvia il responsabile della mensa;
- `utente`: processo che rappresenta un utente della mensa;
- `operatore`: processo che rappresenta un operatore assegnato a una stazione;
- `rifornimento`: processo che rifornisce periodicamente le porzioni di primi e secondi;
- `communication_disorder`: eseguibile esterno che blocca temporaneamente la cassa;
- `aggiungi_utenti`: eseguibile esterno che aggiunge nuovi utenti durante la simulazione.

Il processo responsabile crea le risorse IPC, inizializza la simulazione, genera i processi figli tramite `fork()` ed `execve()`, gestisce il ciclo delle giornate, stampa le statistiche e rimuove le risorse IPC alla fine.

## Struttura delle cartelle

```text
.
├── Makefile
├── config/
│   ├── config_timeout.conf
│   ├── config_overload.conf
│   ├── menu.txt
│   └── parametri.txt
├── include/
│   ├── common.h
│   ├── config.h
│   ├── ipc.h
│   ├── responsabile_mensa.h
│   └── stats.h
└── src/c/
    ├── main.c
    ├── responsabile_mensa.c
    ├── utente.c
    ├── operatore.c
    ├── rifornimento.c
    ├── communication_disorder.c
    ├── aggiungi_utenti.c
    ├── config.c
    ├── ipc.c
    └── stats.c
```

## Meccanismi IPC utilizzati

Il progetto utilizza:

- memoria condivisa, per mantenere lo stato globale della simulazione e le statistiche;
- semafori, per sincronizzare l’accesso alle risorse condivise e rappresentare stazioni, tavoli, ticket e barriere;
- code di messaggi, per la comunicazione tra utenti e operatori e per la sincronizzazione iniziale degli operatori.

Le principali risorse simulate sono:

- stazione primi;
- stazione secondi;
- stazione coffee;
- stazione cassa;
- tavoli;
- corsia ticket;
- gruppi di utenti.

## Compilazione

Per compilare il progetto:

```bash
make clean
make
```

Il `Makefile` compila con i flag:

```bash
-Wall -Wextra -Wvla -Werror -D_GNU_SOURCE
```

Gli eseguibili generati sono:

```text
mensa
utente
operatore
rifornimento
communication_disorder
aggiungi_utenti
```

## Esecuzione della simulazione

### Caso timeout

Per avviare la simulazione con terminazione per timeout:

```bash
./mensa config/config_timeout.conf config/menu.txt
```

### Caso overload

Per avviare la simulazione con terminazione per overload:

```bash
./mensa config/config_overload.conf config/menu.txt
```

Al termine della simulazione viene stampato il report finale con la causa di terminazione.

## Eseguibili esterni

### communication_disorder

Durante l’esecuzione della simulazione, da un secondo terminale è possibile lanciare:

```bash
./communication_disorder config/config_timeout.conf
```

Questo processo si aggancia alle IPC già create dalla simulazione e blocca temporaneamente la cassa per `STOP_DURATION` minuti simulati. Durante il blocco, gli utenti che arrivano alla cassa accumulano maggiore attesa.

### aggiungi_utenti

Durante l’esecuzione della simulazione, da un secondo terminale è possibile lanciare:

```bash
./aggiungi_utenti config/config_timeout.conf config/menu.txt
```

Questo processo aggiunge `N_NEW_USERS` nuovi utenti alla giornata corrente. Gli utenti aggiunti hanno identificativo a partire da `10000` e partecipano alla simulazione senza entrare nella barriera iniziale degli utenti creati dal responsabile.

## Configurazione

I parametri principali sono letti da file di configurazione a tempo di esecuzione. Tra i parametri più importanti ci sono:

- `NOF_USERS`: numero di utenti iniziali;
- `NOF_WORKERS`: numero di operatori;
- `SIM_DURATION`: numero di giornate simulate;
- `MINUTI_GIORNATA`: durata di una giornata in minuti simulati;
- `N_NANO_SECS`: durata reale di un minuto simulato;
- `OVERLOAD_THRESHOLD`: soglia per la terminazione per overload;
- `NOF_WK_SEATS_PRIMI`, `NOF_WK_SEATS_SECONDI`, `NOF_WK_SEATS_COFFEE`, `NOF_WK_SEATS_CASSA`: postazioni delle stazioni;
- `NOF_TABLE_SEATS`: posti ai tavoli;
- `AVG_SRVC_PRIMI`, `AVG_SRVC_SECONDI`, `AVG_SRVC_COFFEE`, `AVG_SRVC_CASSA`: tempi medi di servizio;
- `AVG_REFILL_PRIMI`, `AVG_REFILL_SECONDI`, `MAX_PORZIONI_PRIMI`, `MAX_PORZIONI_SECONDI`: gestione delle porzioni;
- `AVG_REFILL_TIME`: tempo medio di rifornimento;
- `PRICE_PRIMI`, `PRICE_SECONDI`, `PRICE_COFFEE`: prezzi dei prodotti;
- `STOP_DURATION`: durata del blocco della cassa;
- `N_NEW_USERS`: numero di utenti aggiunti dall’eseguibile esterno;
- `MAX_USERS_PER_GROUP`: dimensione massima dei gruppi.

Il file `menu.txt` contiene almeno:

- 2 primi;
- 2 secondi;
- 4 tipi di coffee.

## Statistiche prodotte

Il programma stampa statistiche giornaliere e un report finale. Le statistiche includono:

- utenti serviti e non serviti;
- piatti distribuiti totali e per tipo;
- piatti avanzati;
- tempi medi di attesa per stazione;
- operatori attivi;
- pause effettuate;
- ricavi giornalieri e totali;
- causa di terminazione.

Inoltre, le statistiche finali vengono salvate nel file:

```text
statistiche.csv
```

## Pulizia delle risorse

Al termine della simulazione il processo responsabile:

- attende la terminazione dei figli;
- stacca la memoria condivisa;
- rimuove memoria condivisa e semafori;
- rimuove la coda di messaggi;
- salva le statistiche su CSV.

Per verificare eventuali IPC residue si può usare:

```bash
ipcs
```

## Note di esecuzione

Per test rapidi è consigliabile usare valori bassi di `SIM_DURATION`, `MINUTI_GIORNATA` e `N_NANO_SECS`.

Per rendere visibile l’effetto di `communication_disorder`, è utile impostare un valore abbastanza alto di `STOP_DURATION`.
