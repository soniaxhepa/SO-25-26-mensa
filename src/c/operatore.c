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
}