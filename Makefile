BIN_FILES  =  servidor servidor-rpc
SRC_DIR = src
INCLUDE_DIR = inc
LIB_DIR = lib

RPC_X_FILE     := fecha_hora.x
RPC_GEN_SRCS   := fecha_hora.h      \
                  fecha_hora_clnt.c \
                  fecha_hora_svc.c  \
                  fecha_hora_xdr.c \
				  fecha_hora_server.c \
				 fecha_hora_client.c \
				 Makefile.fecha_hora

CC = gcc
CFLAGS = -I$(INCLUDE_DIR) -Wall -I/usr/include/tirpc
LDFLAGS = -L$(LIB_DIR) -L. -lclaves 
LDLIBS = -lpthread -ldl -ltirpc


all: $(RPC_GEN_SRCS) $(BIN_FILES)
.PHONY: all


$(RPC_GEN_SRCS): $(RPC_X_FILE)
	rm -f $(RPC_GEN_SRCS)
	rpcgen -NM $<

servidor: $(SRC_DIR)/servidor.o $(SRC_DIR)/lines.o $(SRC_DIR)/files.o fecha_hora_clnt.o fecha_hora_xdr.o
	$(CC) $(CFLAGS) $^ -o $@ $(LDLIBS) 

servidor-rpc: servidor-rpc.o fecha_hora_svc.o fecha_hora_xdr.o
	$(CC) $(CFLAGS) $^ -o $@ $(LDLIBS)

$(SRC_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@



clean:
	rm -f $(BIN_FILES) $(SRC_DIR)/*.o *.so $(RPC_GEN_SRCS) *.o 
.SUFFIXES:
.PHONY: clean