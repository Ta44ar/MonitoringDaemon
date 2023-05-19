OBJ = daemon.c fileoperations.c
all: daemon
daemon: $(OBJ)
	gcc $(OBJ) -o daemon -lssl -lcrypto
$(OBJ): fileoperations.h
.PHONY: clean
clean:
	rm -f *.o daemon
