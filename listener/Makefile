CC = gcc
CFLAGS = -Wall -Wextra -std=gnu11
SRC = $(wildcard *.c)
OBJ = $(SRC:.c=.o)
EXEC = atm

all: $(EXEC)

$(EXEC): $(OBJ)
	$(CC) $(OBJ) -o $(EXEC) -g

clean:
	rm -f $(OBJ) $(EXEC)

rebuild: clean all

.PHONY: all clean rebuild