CC=cc
CFLAGS=-Wall -Wextra -std=gnu89 $(OPT) -fPIC -I./src
LDFLAGS=-pthread
SRC=$(wildcard src/*.c)
INC=$(wildcard src/*.h)
OBJ=$(patsubst src/%.c,%.o,$(SRC))

.PHONY: release
release: srv
release: OPT:=-Ofast

.PHONY: debug
debug: srv
debug: OPT:=-O0 -ggdb3

.PHONY: sanitize
sanitize: srv
sanitize: OPT:=-O0 -ggdb3 \
	-fsanitize=address \
	-fsanitize=leak \
	-fsanitize=undefined

srv: $(OBJ)
	$(CC) -o srv $(CFLAGS) $(OBJ) $(LDFLAGS)

%.o: src/%.c $(INC) $(SRC)
	$(CC) -c -o $@ $(CFLAGS) $<

.PHONY: clean
clean:
	rm -f *.o
	rm -f srv
