#include <fcntl.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include<stdio.h>
#include <string.h>
#include <math.h>
#include <netinet/tcp.h>
#include "helpers.h"

typedef struct client_TCP{
	char user_ID[100];
	char IP[50];
	int port;
	int idx;
	int nr_topics;
	int nr_msg;
	struct topic_list *topic_list;
	struct client_UDP *stored_msg_list;
	int disconnected;
}clientTCP;

typedef struct topic_list{
	int unsubscribed;
	char topic[50];
	unsigned char SF;
}topicList;

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

struct udp_message {
    char topic[50];
    unsigned char data_type;
    char payload[1502];
};

struct tcp_message {
    char type[100];
    char topic[100];
    unsigned char SF;
};

int main(int argc,char**argv)
{
	struct client_TCP *tcp_client;
	struct tcp_message tcp_msg;
	tcp_client = calloc(1,sizeof(struct client_TCP));
	int tcp_socket, newtcp_socket, udp_socket;
	char buffer[BUFLEN];
	struct sockaddr_in serv_addr, cli_addr;
	int n, ret, nr_clienti = 0;
	socklen_t clilen;

	fd_set read_fds;	// multimea de citire folosita in select()
	fd_set tmp_fds;		// multime folosita temporar
	int fdmax;			// valoare maxima fd din multimea read_fds


	memset((char *) &serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(atoi(argv[1]));
	serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);

	// se goleste multimea de descripthtonl(ori de cit)ire (read_fds) si multimea temporara (tmp_fds)
	FD_ZERO(&read_fds);
	FD_ZERO(&tmp_fds);

	/*Creare socket TCP*/
	tcp_socket = socket(AF_INET, SOCK_STREAM, 0);
	DIE(tcp_socket < 0, "socket");


	/*Bind socket TCP*/
	ret = bind(tcp_socket, (struct sockaddr *) &serv_addr, sizeof(struct sockaddr));
	DIE(ret < 0, "bind");

	/*Listen socket TCP*/
	ret = listen(tcp_socket, MAX_CLIENTS);
	DIE(ret < 0, "listen");

	/*Creare socket UDP*/
	udp_socket = socket(AF_INET, SOCK_DGRAM, 0);
	DIE(udp_socket < 0, "socket");

	/*Bind socket UDP*/
	ret = bind(udp_socket, (struct sockaddr *) &serv_addr, sizeof(struct sockaddr));
	DIE(ret < 0, "bind");

	// se adauga tcp_socket si udp_socket (socketul pe care se asculta conexiuni) in multimea read_fds
	FD_SET(tcp_socket, &read_fds);
	FD_SET(udp_socket, &read_fds);
	FD_SET(0, &read_fds);
	if(tcp_socket>udp_socket)
		fdmax = tcp_socket;
	else
		fdmax = udp_socket;

	while (1) {

		tmp_fds = read_fds; 
		
		ret = select(fdmax + 1, &tmp_fds, NULL, NULL, NULL);
		DIE(ret < 0, "select");

		//Iterez prin vectorul de clienti si verific daca socketul acestuia se afla in multimea de file-descriptori
		int i = -1, index_client;
		for(int j = 0; j< nr_clienti; j++){
			if(FD_ISSET(tcp_client[j].idx, &tmp_fds)){ 
					i = tcp_client[j].idx;
					index_client = j;
			}
		}
/******************************************************VERIFICARE PRIMIRE MESAJE DE LA CLIENTII TCP********************************************************************************************************************/		
		if(i != -1)
			{ //Primesc date pe unul din socketii de client TCP,
					memset(buffer, 0, BUFLEN);
					n = recv(i, &tcp_msg, sizeof(struct tcp_message), 0);	

					char id_client[100];				
					strcpy(id_client,tcp_client[index_client].user_ID);
					if (n == 0) { // conexiunea s-a inchis
						printf("Client %s disconnected.\n", id_client);
						close(i);						
						// se scoate din multimea de citire socketul inchis
						tcp_client[index_client].disconnected = 1; 
						FD_CLR(i, &read_fds);
					}
					else{
						
						DIE(n < 0, "recv");
						if(strcmp(tcp_msg.type, "subscribe") == 0){ // a dat subscribe la topic
							// verificare daca a mai fost vreodata subscribed la topic
							char topic_sub[100];
							int previously_subscribed = 0;
							strcpy(topic_sub, tcp_msg.topic);
							for(int j = 0; j< tcp_client[index_client].nr_topics; j++)
								if(strcmp(tcp_client[index_client].topic_list[j].topic, topic_sub) == 0){
									tcp_client[index_client].topic_list[j].unsubscribed = 0;
									tcp_client[index_client].topic_list[j].SF = tcp_msg.SF;
									previously_subscribed = 1;
									n = send(i, &tcp_msg, sizeof(struct tcp_message), 0);
									DIE(n < 0, "send");
								}
							//daca nu a mai fost niciodata subscribed la acel topic, il adaugam la lista de topicuri
							if(previously_subscribed == 0){
								memcpy(tcp_client[index_client].topic_list[tcp_client[index_client].nr_topics].topic, tcp_msg.topic, strlen(tcp_msg.topic));
								tcp_client[index_client].topic_list[tcp_client[index_client].nr_topics].SF = tcp_msg.SF;
								tcp_client[index_client].nr_topics++;
								tcp_client[index_client].topic_list = realloc(tcp_client[index_client].topic_list,(tcp_client[index_client].nr_topics+1)*sizeof(struct topic_list));					
								n = send(i, &tcp_msg, sizeof(struct tcp_message), 0);
								DIE(n < 0, "send");
							}
						}
						else if(strcmp(tcp_msg.type, "unsubscribe") == 0){ // a dat unsubscribe la topic
							char topic_unsub[100];
							strcpy(topic_unsub, tcp_msg.topic);
							for(int j = 0; j< tcp_client[index_client].nr_topics; j++)
								if(strcmp(tcp_client[index_client].topic_list[j].topic, topic_unsub) == 0){
									tcp_client[index_client].topic_list[j].unsubscribed = 1;
								}
							n = send(i, &tcp_msg, sizeof(struct tcp_message), 0);
							DIE(n < 0, "send");
						}
					} 
				}
/******************************************************VERIFICARE CONECTARE CLIENT TCP*******************************************************************************************************************/
			if (FD_ISSET(tcp_socket, &tmp_fds) && i == -1) {
					// a venit o cerere de conexiune pe socketul inactiv (cel cu listen),
					// pe care serverul o accepta
					clilen = sizeof(cli_addr);
					newtcp_socket = accept(tcp_socket, (struct sockaddr *) &cli_addr, &clilen);
					DIE(newtcp_socket < 0, "accept");

					// primeste user ID
					memset(buffer, 0, BUFLEN);
					n = recv(newtcp_socket, buffer, sizeof(buffer), 0);
					DIE(n < 0, "recv");

					int user_taken = 0;
					for(int j = 0; j< nr_clienti; j++)
						if(strcmp(tcp_client[j].user_ID, buffer) == 0 && tcp_client[j].disconnected == 0){
								user_taken = 1;
								char wrong_id[BUFLEN];
								memset(wrong_id, 0, BUFLEN);
								strcpy(wrong_id, "wrong_id");
								n = send(newtcp_socket, &wrong_id, sizeof(wrong_id), 0);
								DIE(n < 0, "send");
						}
					if(user_taken == 0){
					//caut daca clientul este unul vechi
					int client_existent = 0;
					for(int j = 0; j< nr_clienti; j++)
						if(strcmp(tcp_client[j].user_ID, buffer) == 0){
							client_existent = 1;
							tcp_client[j].disconnected = 0;
							tcp_client[j].idx = newtcp_socket;
							printf("Client %s reconnected.\n", tcp_client[j].user_ID);

							//adaug socketul intors la multimea descriptorilor de citire
							FD_SET(newtcp_socket, &read_fds);
							if (newtcp_socket > fdmax) { 
								fdmax = newtcp_socket;
							}

					char ok_id[BUFLEN];
					memset(ok_id, 0, BUFLEN);
					strcpy(ok_id, "ok_id");
					n = send(newtcp_socket, &ok_id, sizeof(ok_id), 0);
					DIE(n < 0, "send");
							//verific de mesaje
							
							int stored = 0;
							for(int l = 0; l< tcp_client[j].nr_topics; l++)
								if(tcp_client[j].topic_list[l].SF == 1)
									stored = 1;

							if(stored == 1 && tcp_client[j].nr_msg > 0){
								for(int l = 0; l<tcp_client[j].nr_msg; l++){
									int sock_TCP = tcp_client[j].idx;
									n = send(sock_TCP, &tcp_client[j].stored_msg_list[l], sizeof(struct client_UDP), 0);
									DIE(n < 0, "send");
								}
								free(tcp_client[j].stored_msg_list);
								tcp_client[j].nr_msg = 0;
								tcp_client[j].stored_msg_list = calloc(1,sizeof(struct client_UDP));
							}
						}
					//daca este un client nou, il adaug
					if(client_existent == 0){
						int flag = 1;
						int result = setsockopt(newtcp_socket, IPPROTO_TCP, TCP_NODELAY, (char *) &flag, sizeof(int));
 						DIE(result < 0, "neagle");
 		
						strcpy(tcp_client[nr_clienti].user_ID, buffer);
						strcpy(tcp_client[nr_clienti].IP, inet_ntoa(cli_addr.sin_addr));
						tcp_client[nr_clienti].port = ntohs(cli_addr.sin_port);
						tcp_client[nr_clienti].idx = newtcp_socket;

						// se adauga noul socket intors de accept() la multimea descriptorilor de citire
						FD_SET(newtcp_socket, &read_fds);
						if (newtcp_socket > fdmax) { 
							fdmax = newtcp_socket;
						}

						 printf("New client %s connected from  %s : %d\n", tcp_client[nr_clienti].user_ID, tcp_client[nr_clienti].IP, tcp_client[nr_clienti].port);
						 tcp_client[nr_clienti].topic_list = calloc(1,sizeof(struct topic_list));
						 tcp_client[nr_clienti].stored_msg_list = calloc(1,sizeof(struct client_UDP));
						 tcp_client[nr_clienti].nr_topics = 0;
						 nr_clienti++;
						 tcp_client = realloc(tcp_client, (nr_clienti+1)*sizeof(struct client_TCP));

						 char ok_id[BUFLEN];
						 memset(ok_id, 0, BUFLEN);
					   	 strcpy(ok_id, "ok_id");
						n = send(newtcp_socket, &ok_id, sizeof(ok_id), 0);
						DIE(n < 0, "send");
					}
				}
			}
/******************************************************VERIFICARE MESAJ PRIMIT PE UDP********************************************************************************************************************/

				if (FD_ISSET(udp_socket, &tmp_fds)){ // Socket UDP
					clilen = sizeof(cli_addr);
					struct udp_message udp_msg;
					struct sockaddr_in from_udp;
					struct client_UDP udp_client;

					int r;
					socklen_t length = sizeof(from_udp);

					memset(udp_msg.payload, 0, 1500);
					r = recvfrom(udp_socket, &udp_msg, sizeof(struct udp_message), 0, (struct sockaddr*) &from_udp, &length);
					DIE(r<0, "receive udp_msg");	

						strcpy(udp_client.topic, udp_msg.topic);
						udp_client.data_type = udp_msg.data_type;

						if(udp_msg.data_type == 0){ //INT
							int semn = udp_msg.payload[0];
							int int_value = ntohl(udp_msg.payload[1]) + ((ntohl(udp_msg.payload[2])>>24)<<16) + ((ntohl(udp_msg.payload[3])>>24)<<8) + (ntohl(udp_msg.payload[4])>>24);
							if(semn == 1)
								int_value *= -1;
							udp_client.int_value = int_value;
						}
						if(udp_msg.data_type == 1){//SHORT
							int modul = (ntohl(udp_msg.payload[0])>>24<<8) + (ntohl(udp_msg.payload[1])>>24);
							double short_value = (((double)modul) / 100);
							udp_client.short_value = short_value;
						}
						if(udp_msg.data_type == 2){//FLOAT
							int semn = udp_msg.payload[0];
							int modul = ntohl(udp_msg.payload[1]) + ((ntohl(udp_msg.payload[2])>>24)<<16) + ((ntohl(udp_msg.payload[3])>>24)<<8) + (ntohl(udp_msg.payload[4])>>24);
							if(semn == 1)
								modul *= -1;
							int exponent = ntohl(udp_msg.payload[5])>>24;
							int putere = pow(10,exponent);
							double float_value = (((double)modul) / putere);
							udp_client.float_value = float_value;

						}
						if(udp_msg.data_type == 3){//STRING
							strcpy(udp_client.string_value, udp_msg.payload);
						}

					udp_client.port_udp = htons(from_udp.sin_port);
					strcpy(udp_client.ip_udp, inet_ntoa(from_udp.sin_addr));

					// Trimiterea mesajului udp receptional la clientii tcp abonati
					for(int j = 0; j< nr_clienti; j++){
						for(int k = 0; k< tcp_client[j].nr_topics; k++){
							if(strcmp(tcp_client[j].topic_list[k].topic, udp_client.topic) == 0 && tcp_client[j].topic_list[k].unsubscribed == 0){
								if(tcp_client[j].topic_list[k].SF == 1 && tcp_client[j].disconnected == 0){
									i = tcp_client[j].idx;
									n = send(i, &udp_client, sizeof(struct client_UDP), 0);
									DIE(n < 0, "send");
								}
								else if(tcp_client[j].topic_list[k].SF == 1 && tcp_client[j].disconnected == 1){
									//salvez mesajul ca sa trimit mai tarziu
									strcpy(tcp_client[j].stored_msg_list[tcp_client[j].nr_msg].topic ,udp_client.topic);
									strcpy(tcp_client[j].stored_msg_list[tcp_client[j].nr_msg].ip_udp ,udp_client.ip_udp);
									strcpy(tcp_client[j].stored_msg_list[tcp_client[j].nr_msg].string_value ,udp_client.string_value);
									tcp_client[j].stored_msg_list[tcp_client[j].nr_msg].int_value = udp_client.int_value;
									tcp_client[j].stored_msg_list[tcp_client[j].nr_msg].short_value = udp_client.short_value;
									tcp_client[j].stored_msg_list[tcp_client[j].nr_msg].float_value = udp_client.float_value;
									tcp_client[j].stored_msg_list[tcp_client[j].nr_msg].data_type = udp_client.data_type;
									tcp_client[j].stored_msg_list[tcp_client[j].nr_msg].port_udp = udp_client.port_udp;
									tcp_client[j].nr_msg++;
									tcp_client[j].stored_msg_list = realloc(tcp_client[j].stored_msg_list, (tcp_client[j].nr_msg+1)*sizeof(struct client_UDP));
								}

								else if(tcp_client[j].topic_list[k].SF == 0 && tcp_client[j].disconnected == 0){
									i = tcp_client[j].idx;
									n = send(i, &udp_client, sizeof(struct client_UDP), 0);
									DIE(n < 0, "send");
								}
							}
						}
					}
					 
				}
/******************************************************VERIFICARE COMANDA EXIT********************************************************************************************************************/
			if (FD_ISSET(0, &tmp_fds)) {
				memset(buffer, 0, BUFLEN);
				fgets(buffer, BUFLEN - 1, stdin);
				// verific exit
				if(strcmp(buffer, "exit\n") == 0){
					for(int j=0; j<nr_clienti; j++){
						if(tcp_client[j].disconnected == 0){
							printf("Client %s disconnected.\n", tcp_client[j].user_ID);
							close(tcp_client[j].idx);						
							// se scoate din multimea de citire socketul inchis
							tcp_client[j].disconnected = 1; 
							FD_CLR(tcp_client[j].idx, &read_fds);
						}
						else{
							close(tcp_client[j].idx);						
							// se scoate din multimea de citire socketul inchis
							FD_CLR(tcp_client[j].idx, &read_fds);
						}
					}

					printf("Shut down server\n");
					// close(tcp_socket);
					break;
				}
				memset(buffer, 0, BUFLEN);
			}
	}

	/*Inchidere socket*/
	close(tcp_socket);
	/*Inchidere socket*/	
	close(udp_socket);
	/*Dezalocari*/
	for(int j=0; j<nr_clienti; j++){
		free(tcp_client[j].topic_list);
		free(tcp_client[j].stored_msg_list);
	}
	free(tcp_client);
	return 0;

}