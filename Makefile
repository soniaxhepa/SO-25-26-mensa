CC     = gcc
CFLAGS = -Wall -Wextra -Wvla -Werror -D_GNU_SOURCE -I./include

COMMON = src/c/config.c src/c/ipc.c src/c/stats.c

all: mensa utente operatore communication_disorder rifornimento aggiungi_utenti

mensa: src/c/main.c src/c/responsabile_mensa.c $(COMMON)
	$(CC) $(CFLAGS) $^ -o $@

utente: src/c/utente.c $(COMMON)
	$(CC) $(CFLAGS) $^ -o $@

operatore: src/c/operatore.c $(COMMON)
	$(CC) $(CFLAGS) $^ -o $@

communication_disorder: src/c/communication_disorder.c $(COMMON)
	$(CC) $(CFLAGS) $^ -o $@

rifornimento: src/c/rifornimento.c $(COMMON)
	$(CC) $(CFLAGS) $^ -o $@

aggiungi_utenti: src/c/aggiungi_utenti.c $(COMMON)
	$(CC) $(CFLAGS) $^ -o $@

clean:
	rm -f mensa utente operatore communication_disorder rifornimento aggiungi_utenti statistiche.csv
