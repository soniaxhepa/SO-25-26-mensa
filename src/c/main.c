#include "../../include/responsabile_mensa.h"

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Uso: %s <config_file> <menu_file>\n"
            "Esempio:\n"
            "  %s config/config_timeout.conf  config/menu.txt\n"
            "  %s config/config_overload.conf config/menu.txt\n", argv[0], argv[0], argv[0]);
        return 1;
    }
    return avvia_responsabile_mensa(argv[1], argv[2]);
}


