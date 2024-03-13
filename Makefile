SRC_DIR=src
HEADER_DIR=include
OBJ_DIR=obj

MPI_CC=mpicc   # Use mpicc as the MPI wrapper compiler
CC=gcc
CFLAGS=-O0 -fsanitize=address -ggdb -Wall -Wextra -I$(HEADER_DIR) 
MPI_CFLAGS=-O0 -fsanitize=address -ggdb -Wall -Wextra -I$(HEADER_DIR)  # MPI-specific flags
LDFLAGS=-lm

SRC= dgif_lib.c \
	egif_lib.c \
	gif_err.c \
	gif_font.c \
	gif_hash.c \
	gifalloc.c \
	main.c \
	openbsd-reallocarray.c \
	quantize.c

OBJ= $(OBJ_DIR)/dgif_lib.o \
	$(OBJ_DIR)/egif_lib.o \
	$(OBJ_DIR)/gif_err.o \
	$(OBJ_DIR)/gif_font.o \
	$(OBJ_DIR)/gif_hash.o \
	$(OBJ_DIR)/gifalloc.o \
	$(OBJ_DIR)/main.o \
	$(OBJ_DIR)/openbsd-reallocarray.o \
	$(OBJ_DIR)/quantize.o

all: $(OBJ_DIR) sobelf

$(OBJ_DIR):
	mkdir $(OBJ_DIR)

$(OBJ_DIR)/%.o : $(SRC_DIR)/%.c
	$(MPI_CC) $(MPI_CFLAGS) -c -o $@ $^

sobelf: $(OBJ)
	$(MPI_CC) $(MPI_CFLAGS) -o $@ $^ $(LDFLAGS)

clean:
	rm -f sobelf $(OBJ)
