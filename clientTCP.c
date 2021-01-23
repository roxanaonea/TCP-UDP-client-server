#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include "helpers.h"

struct tcp_message {
    char type[100];
    char topic[100];
    unsigned char SF;
};

typedef struct client_UDP{
	char topic[50];
    unsigned char data_type;
    int int_value;	
    double short_value;
    double float_value;
    char string_value[1502];
    unsigned short port_udp;
    char ip_udp[50];
}clientUDP;

int main(int argc, char *argv[])
{
	struct tcp_message tcp_msg;
	struct client_UDP udp_client;
	int sockfd, n, ret;
	struct sockaddr_in serv_addr;
	char buffer[BUFLEN];

	fd_set read_fds;	// multimea de citire folosita in select()
	fd_set tmp_fds;		// multime folosita temporar
	int fdmax;			// valoare maxima fd din multimea read_fds
	// se goleste multimea de descriptori de citire (read_fds) si multimea temporara (tmp_fds)
	FD_ZERO(&read_fds);
	FD_ZERO(&tmp_fds);

	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	DIE(sockfd < 0, "socket");

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(atoi(argv[3]));
	ret = inet_aton(argv[2], &serv_addr.sin_addr);
	DIE(ret == 0, "inet_aton");

	ret = connect(sockfd, (struct sockaddr*) &serv_addr, sizeof(serv_addr));
	DIE(ret < 0, "connect");

	//trimitere user_ID
	strcpy(buffer, argv[1]);
	strcat(buffer, "\n");
	n = send(sockfd, buffer, strlen(argv[1]), 0);
	DIE(n < 0, "send");
	memset(buffer, 0, BUFLEN);

	//se deide daca user_ID-ul este unul valabil sau nu
	int bytes_received = recv(sockfd, &buffer, BUFLEN, 0);
	DIE(bytes_received < 0, "recv");
	if(strcmp(buffer, "wrong_id") == 0){
		printf("Error: User ID aready in use!\n");
		close(sockfd);
		exit(0);
	}

	// se adauga noul file descriptor (socketul pe care se asculta conexiuni) in multimea read_fds
	FD_SET(sockfd, &read_fds);
	FD_SET(0, &read_fds);
	fdmax = sockfd;


	while (1) {
		tmp_fds = read_fds; 
		ret = select(fdmax + 1, &tmp_fds, NULL, NULL, NULL);
		DIE(ret < 0, "select");
  		
		if (strncmp(buffer, "exit", 4) == 0) {
			break;
		}
/*AM PRIMIT COMANDA DE LA TASTATURA*/
		if(FD_ISSET(0, &tmp_fds)){
			// se citeste de la tastatura
			memset(buffer, 0, BUFLEN);
			fgets(buffer, BUFLEN , stdin);
			sscanf(buffer, "%s %s %hhd", tcp_msg.type, tcp_msg.topic, &tcp_msg.SF);

			// verific exit
				if(strcmp(buffer, "exit\n") == 0){
						memset(buffer, 0, BUFLEN);
						n = send(sockfd, buffer, 0, 0);
						DIE(n < 0, "send");
						memset(buffer, 0, BUFLEN);
						break;
				}
			// verific subscribe
				if(strcmp(tcp_msg.type, "subscribe") == 0){
					n = send(sockfd, &tcp_msg, sizeof(struct tcp_message), 0);
					DIE(n < 0, "send");
				}
			// verific unsubscribe
				if(strcmp(tcp_msg.type, "unsubscribe") == 0){
					n = send(sockfd, &tcp_msg, sizeof(struct tcp_message), 0);
					DIE(n < 0, "send");
				}	
			//primire raspuns
				int bytes_received = recv(sockfd, &tcp_msg, BUFLEN, 0);
				DIE(bytes_received < 0, "send");
				printf("%sd %s.\n", tcp_msg.type, tcp_msg.topic);
		}
/*AM RECEPTIONAT MESAJ PE SOCKET*/
		else if(FD_ISSET(sockfd, &tmp_fds)){
			// receptionare mesaj de la server			
			int bytes_received = recv(sockfd, &udp_client, sizeof(struct client_UDP), 0);
			DIE(bytes_received < 0, "send");

			// verificam daca s-a inchis serverul
			if(bytes_received == 0)
			{
				printf("Server shut down.\n");
				break;
			}

			// verificam ce tip de data am primit
			if(udp_client.data_type == 0){
				printf("%s:%d - %s - %s - %d\n", udp_client.ip_udp, udp_client.port_udp, udp_client.topic, "INT", udp_client.int_value);
			}
			if(udp_client.data_type == 1){
				printf("%s:%d - %s - %s - %2f\n", udp_client.ip_udp, udp_client.port_udp, udp_client.topic, "SHORT_REAL", udp_client.short_value);
			}
			if(udp_client.data_type == 2){
				printf("%s:%d - %s - %s - %f\n", udp_client.ip_udp, udp_client.port_udp, udp_client.topic, "FLOAT", udp_client.float_value);
			}
			if(udp_client.data_type == 3){
				printf("%s:%d - %s - %s - %s\n", udp_client.ip_udp, udp_client.port_udp, udp_client.topic, "STRING", udp_client.string_value);
			}
    	}       

	}

	close(sockfd);

	return 0;
}