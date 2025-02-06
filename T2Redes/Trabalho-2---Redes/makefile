# Compilador
CC = gcc

# Flags de compilação
CFLAGS = -Wall -Wextra -std=c99 -O2

# Nomes dos executáveis
CLIENT_EXEC = cliente
SERVER_EXEC = servidor

# Arquivos fonte
CLIENT_SRC = cliente.c rdt_2.2.c
SERVER_SRC = servidor.c rdt_2.2.c

# Regra padrão (compila tudo)
all: $(CLIENT_EXEC) $(SERVER_EXEC)

# Regra para compilar o cliente
$(CLIENT_EXEC): $(CLIENT_SRC)
	$(CC) $(CFLAGS) -o $@ $^

# Regra para compilar o servidor
$(SERVER_EXEC): $(SERVER_SRC)
	$(CC) $(CFLAGS) -o $@ $^

# Limpeza dos arquivos gerados
clean:
	rm -f $(CLIENT_EXEC) $(SERVER_EXEC)

# Instruções de uso
.PHONY: all clean