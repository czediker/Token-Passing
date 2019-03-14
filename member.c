#include <pthread.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>
#include <assert.h>

typedef struct{
	char name[50];
	char ip[50];
	int port;
} Machine;

char buffer[256];
int somethingToSend = 0;
int num_machines = 0;
int me = 0;
int token;
Machine *linux_machines;
pthread_mutex_t lock;
pthread_t taker, tokener;
void *recieveToken(void *x){

	/* Setup new socket for recieving token at specified port+1. */
	int sock;
	int bytes_read;
	struct sockaddr_in server_addr , client_addr;
	socklen_t addr_len;
	if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
		perror("Socket");
		exit(1);
	}
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(linux_machines[me].port + 1);
	server_addr.sin_addr.s_addr = INADDR_ANY;
	if (bind(sock,(struct sockaddr *)&server_addr, sizeof(struct sockaddr)) == -1){
		perror("Bind");
		exit(1);
	}
	addr_len = sizeof(struct sockaddr);
	
	/* Wait for token to be recieved */
	while(1){
		bytes_read = recvfrom(sock,&token,sizeof(int),0, (struct sockaddr*)&client_addr, &addr_len);
		printf("%i|Recieved new token: %i\n",me,token);
	}
}

void *takeInfo(void *x){
	/* Wait on new message to send. */
	while(1){
		scanf("%s",&buffer);
		pthread_mutex_lock(&lock);
		somethingToSend = 1;
		pthread_mutex_unlock(&lock);
		sleep(5);
	}
}

int main(int argc, char *argv[]){
	/* Basic setup */
	sleep(15);
	if(argc != 5){
		printf("Usage <this machine id> <total machines> <machines file> <token file>\n");
		return 1;
	}
	me = atoi(argv[1]);
	num_machines = atoi(argv[2]);
	char *machines = argv[3];
	char *tokens = argv[4];
	linux_machines = (Machine*)malloc(sizeof(Machine)*num_machines);
	pthread_mutex_init(&lock,NULL);
	
	/* Set up socket for communicating with other ring members. */
	int sock;
	struct sockaddr_in server_addr[num_machines];
	struct hostent *host;
	socklen_t addr_len;
	host= (struct hostent *)gethostbyname((char *)"127.0.0.1");
	if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) == -1){
		perror("socket");
		exit(1);
	}
	
	/* Fetch other ring members. */
	FILE *fp = NULL;
	fp=fopen(machines,"r");
	int i = 0;
	for(i = 0; i <num_machines;i++){
		char _name[50];
		char _ip[50];
		int _port;
		fscanf(fp,"%s %s %i",_name,&_ip,&_port);
		memcpy(linux_machines[i].name,_name, strlen(_name)+1);
		memcpy(linux_machines[i].ip, _ip, strlen(_ip)+1);
		linux_machines[i].port = _port;
		host = (struct hostent *)gethostbyname((char *) linux_machines[i].ip);
		server_addr[i].sin_family = AF_INET;
		server_addr[i].sin_port = htons(((int)_port) + 1);
		server_addr[i].sin_addr = *((struct in_addr *)host->h_addr);
	}
	fclose(fp);	
	/* Manage starting token */
	FILE *fp2 = NULL;
	fp2=fopen(tokens,"r");
	fscanf(fp2,"%i",&token);
	fp2=fopen(tokens,"w");
	fprintf(fp2,"%i",0);
	fclose(fp2);
	printf("%i|Given token: %i\n",me,token);
	pthread_create(&taker,NULL,takeInfo,NULL);
	pthread_create(&tokener,NULL,recieveToken,NULL);

	/* Setup destination socket. */
	int recvsock;
        struct sockaddr_in recvserver_addr;
        struct hostent *recvhost;
        socklen_t recvserver_addr_len;
        recvhost= (struct hostent *)gethostbyname((char *)"127.0.0.1");
	if ((recvsock = socket(AF_INET, SOCK_DGRAM, 0)) == -1){
                perror("socket");
                exit(1);
        }

	/* Enter main logic loop */
	while(1){
		sleep(5);
		if(somethingToSend != 0 && token != 0){
			printf("%i|Sending out data\n",me);
			//Send out data
			sendto(sock, &buffer, sizeof(buffer), 0,(struct sockaddr *)&recvserver_addr, sizeof(struct sockaddr));
			//Pass token off
			printf("%i|Passing off token\n",me);
                	sendto(sock, &token, sizeof(int), 0,(struct sockaddr *)&server_addr[(me+1)%num_machines], sizeof(struct sockaddr));
	                token = 0;
        	        somethingToSend = 0;
		}else if(token != 0){
			printf("%i|Passing off token\n",me);
                        sendto(sock, &token, sizeof(int), 0,(struct sockaddr *)&server_addr[(me+1)%num_machines], sizeof(struct sockaddr));
                        token = 0;
                        somethingToSend = 0;
		}else{
			//Do not have token. Do nothing
		}
		sleep(5);
	}
}
