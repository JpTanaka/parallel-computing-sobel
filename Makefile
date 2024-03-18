SRC_DIR=src
HEADER_DIR=include
OBJ_DIR=obj

MPI_CC=mpicc   # Use mpicc as the MPI wrapper compiler
CC=gcc
NVCC=nvcc  # Use nvcc for CUDA compilation

CFLAGS=-O3   -I$(HEADER_DIR) 
MPI_CFLAGS=-O3 -fopenmp -I$(HEADER_DIR)  # MPI-specific flags
# MPI_CFLAGS=-O3 -fsanitize=address -I$(HEADER_DIR)  # MPI-specific flags
CUDA_CFLAGS=-O3  -I$(HEADER_DIR) -I/usr/local/cuda/include -L/usr/local/cuda/lib64 -lcudart -lcuda
LDFLAGS=-lm 

SRC= dgif_lib.c \
    egif_lib.c \
    gif_err.c \
    gif_font.c \
    gif_hash.c \
    gifalloc.c \
    cuda_functions.cu \
    main.c \
    openbsd-reallocarray.c \
    quantize.c 

OBJ= $(OBJ_DIR)/dgif_lib.o \
    $(OBJ_DIR)/egif_lib.o \
    $(OBJ_DIR)/gif_err.o \
    $(OBJ_DIR)/gif_font.o \
    $(OBJ_DIR)/gif_hash.o \
    $(OBJ_DIR)/gifalloc.o \
    $(OBJ_DIR)/cuda_functions.o \
    $(OBJ_DIR)/main.o \
    $(OBJ_DIR)/openbsd-reallocarray.o \
    $(OBJ_DIR)/quantize.o \

all: $(OBJ_DIR) sobelf

$(OBJ_DIR):
	mkdir $(OBJ_DIR)

$(OBJ_DIR)/cuda_functions.o: $(SRC_DIR)/cuda_functions.cu
	$(NVCC) $(CUDA_CFLAGS) -Iinclude -c -o $@ $<

$(OBJ_DIR)/%.o : $(SRC_DIR)/%.c
	$(MPI_CC) $(MPI_CFLAGS) -c -o $@ $< -lcudart

sobelf: $(OBJ)
	$(MPI_CC) $(MPI_CFLAGS) $(CUDA_CFLAGS) -o $@ $^ $(LDFLAGS) -lcudart

clean:
	rm -f sobelf $(OBJ)
