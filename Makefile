all: server client

# Compileaza server.c
server: server.c
	gcc -Wall -g -o server server.c -lm	
# Compileaza client.c
client: clientTCP.c
	gcc -Wall -g -o subscriber clientTCP.c -lm
.PHONY: clean run_server run_client

clean:
	rm -f server subscriber
