BIN_FILES  =  servidor 
SRC_DIR = src
INCLUDE_DIR = inc
LIB_DIR = lib

CC = gcc
CFLAGS = -I$(INCLUDE_DIR) -Wall -g
LDFLAGS = -L$(LIB_DIR) -L. -lclaves
LDLIBS = -lpthread -lrt

all: $(BIN_FILES)
.PHONY: all



servidor: $(SRC_DIR)/servidor.o $(SRC_DIR)/lines.o $(SRC_DIR)/files.o
	$(CC) $(CFLAGS) $^ -o $@ $(LDLIBS) 


$(SRC_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(BIN_FILES) $(SRC_DIR)/*.o *.so

.SUFFIXES:
.PHONY: clean