#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <pthread.h>
#include <sys/types.h>
#include <signal.h>
#include <ctype.h>


#define MAX_CLIENTS 100
#define BUFFER_SZ 6068
#define LENGTH 6068
#define CRCNUM 200
volatile sig_atomic_t flag = 0;

FILE *logsfile;
FILE *listfile;

static _Atomic unsigned int cli_count = 0;
static int uid = 10;

char *ip = "127.0.0.1";

//struct of message
struct message_struct
{
	char recp[LENGTH];
    char message[LENGTH];
    unsigned int checksum;
    char CRCMessage[LENGTH+32];
} msg;


/* Client structure */
typedef struct{
	struct sockaddr_in address;
	int sockfd;
	int uid;
	char name[32];
} client_t;

client_t *clients[MAX_CLIENTS];

pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;

void str_overwrite_stdout() {
    printf("\r%s", "> ");
    fflush(stdout);
}

void catch_ctrl_c_and_exit(int sig) {
    	flag = 1;
	printf("Bye\n");
	fclose(logsfile);
	exit(0);
}

void str_trim_lf (char* arr, int length) {
  int i;
  for (i = 0; i < length; i++) { // trim \n
    if (arr[i] == '\n') {
      arr[i] = '\0';
      break;
    }
  }
}

//checksum
unsigned char checksum(char *string) {
       	unsigned char sum = 0;
  for (int i = 0; i < strlen(string); i++) {
    sum += string[i];
  }
  return sum % 256;
}


// CRC Calc
char *crc(char *message)
{
    
        unsigned char sum = 0;
        for (int i = 0; i < strlen(message); i++) {
                sum += message[i];
        }
        sum = sum % CRCNUM;
	/*if ((int)sum >= 200){
		
	}*/
	//printf("\nthe sum  of crc = %d\n",sum);
        char *crcmsg = malloc(LENGTH+4);
        //printf("\nint sum = %d\t char = %c\t length = %ld\n",sum,(char)sum,strlen(message));
        sprintf(crcmsg,"%s%c",message,(char)sum);
        //printf("\n\nCRC MESSG : %s\tlen = %ld\n",crcmsg,strlen(crcmsg));
        //printf("\n\nmessage : %s\n\n\n",message);
        return crcmsg;
}


void print_client_addr(struct sockaddr_in addr){
    printf("%d.%d.%d.%d",
        addr.sin_addr.s_addr & 0xff,
        (addr.sin_addr.s_addr & 0xff00) >> 8,
        (addr.sin_addr.s_addr & 0xff0000) >> 16,
        (addr.sin_addr.s_addr & 0xff000000) >> 24);
}



/* Add clients to queue */
void queue_add(client_t *cl){
	pthread_mutex_lock(&clients_mutex);

	for(int i=0; i < MAX_CLIENTS; ++i){
		if(!clients[i]){
			clients[i] = cl;
			break;
		}
	}
	

	pthread_mutex_unlock(&clients_mutex);
}

/* Remove clients to queue */
void queue_remove(int uid){
	pthread_mutex_lock(&clients_mutex);

	for(int i=0; i < MAX_CLIENTS; ++i){
		if(clients[i]){
			if(clients[i]->uid == uid){
				clients[i] = NULL;
				break;
			}
		}
	}
	

	pthread_mutex_unlock(&clients_mutex);
}

/* Send message to all clients except sender */
void send_message(char *s, int uid){
	pthread_mutex_lock(&clients_mutex);

	for(int i=0; i<MAX_CLIENTS; ++i){
		if(clients[i]){
			if(clients[i]->uid != uid){
				if(write(clients[i]->sockfd, s, strlen(s)) < 0){
					perror("ERROR: write to descriptor failed");
					break;
				}
			}
		}
	}

	pthread_mutex_unlock(&clients_mutex);
}

void send_message_private(char *s, int uid){
	pthread_mutex_lock(&clients_mutex);

	for(int i=0; i<MAX_CLIENTS; ++i){
		if(clients[i]){
			if(clients[i]->uid == uid){
				if(write(clients[i]->sockfd, s, strlen(s)) < 0){
					perror("ERROR: write to descriptor failed");
					break;
				}
			}
		}
	}

	pthread_mutex_unlock(&clients_mutex);
}


void update_user_list() {
    listfile = fopen("list.txt", "w");
    if (listfile == NULL) {
        perror("ERROR: Could not open list.txt");
        return;
    }

//    fprintf(listfile, "=== ACTIVE USERS ===\n");

    pthread_mutex_lock(&clients_mutex);

    for (int i = 0; i < MAX_CLIENTS; ++i) {
        if (clients[i]) {
            fprintf(listfile, "%s\n", clients[i]->name);
        }
    }

    pthread_mutex_unlock(&clients_mutex);

    fclose(listfile);
}

int getRandomValue() {
    return rand() % 2;  // Generates a random number either 0 or 1
}

client_t *get_client_by_name(char *name)
{
    pthread_mutex_lock(&clients_mutex);

    for (int i = 0; i < MAX_CLIENTS; ++i)
    {
        if (clients[i] && strcmp(clients[i]->name, name) == 0)
        {
            client_t *client = clients[i];
            pthread_mutex_unlock(&clients_mutex);
            return client;
        }
    }

    pthread_mutex_unlock(&clients_mutex);
    return NULL;
}






/* Handle all communication with the client */
void *handle_client(void *arg){
	time_t t = time(NULL);
        //char fName[200];
        struct tm tm = *localtime(&t);

        //sprintf(fName,"%d-%02d-%02d %02d:%02d:%02d", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
	
	//char buff_out[BUFFER_SZ];
	struct message_struct buff_out;
	char msg[BUFFER_SZ+36]; //+36
	char name[32];
	int leave_flag = 0;

	cli_count++;
	client_t *cli = (client_t *)arg;

	// Name
	if(recv(cli->sockfd, name, 32, 0) <= 0 || strlen(name) <  2 || strlen(name) >= 32-1){
		printf("Didn't enter the name.\n");
		leave_flag = 1;
	} else{
		strcpy(cli->name, name);
		sprintf(buff_out.message, "%s has joined\n", cli->name);
		char joined[300];
		sprintf(joined,"%s has joined %d-%02d-%02d %02d:%02d:%02d\n",cli->name,tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
		//printf("%s",joined);
		fprintf(logsfile,"%s",joined);
		printf("%d %s",cli->uid, buff_out.message);
		update_user_list();
//		send_message(buff_out.message, cli->uid);
	}

	bzero(buff_out.message, BUFFER_SZ);
	//int ch = -1;
	while(1){
		
		if (leave_flag) {
			break;
		}
		

		//int receive = recv(cli->sockfd, buff_out, BUFFER_SZ, 0);
		int receive = recv(cli->sockfd , &buff_out, sizeof(buff_out), 0);
//		if(strcmp(buff_out.recp, "all") == 0){
//			printf("tsook omaha lak shta8al");
//		}
		//printf("buff_out : %s\n",buff_out);
		if (receive > 0){
			if(strlen(buff_out.message) > 0){ //buff_out
			
				
//				printf("%d , %d\n",checksum(buff_out.message),buff_out.checksum);
				if ((unsigned int)checksum(buff_out.message) == (unsigned int)buff_out.checksum){ // ch
	
					
					//printf("the number of checknumsum is : %d",checknumsum(checksum(buff_out),buff_out));
					//printf("\n************\nmessage = %s\nchecksum = %d\nCRC = %s\n***********\n",buff_out.message,buff_out.checksum,buff_out.CRCMessage);
					sprintf(msg , "%s: %s" , cli->name , buff_out.message);
					client_t *recp = get_client_by_name(buff_out.recp);
					
					 int x = getRandomValue();
					 if(x==1){
					 	strcpy(buff_out.CRCMessage,"corrupted");
					 }
					
					if (strcmp(crc(buff_out.message),buff_out.CRCMessage) != 0){
						printf("Error in CRC \n original msg :%s, corrupted msg:corrupted\n",buff_out.message);
						
					}else{
						
					
						
						str_trim_lf(buff_out.message, strlen(buff_out.message)); // buff_out
//						printf("%s -> %s\n", buff_out.message, cli->name); //message
						if(strcmp(buff_out.recp, "all") == 0){
							printf("clinet checksum : %d & server checksum : %d\n",buff_out.checksum,checksum(buff_out.message));
							printf("No Error in CRC\n");
							printf("%s -> %s\n", buff_out.message, cli->name);
							send_message(msg, cli->uid);
						}else if(strcmp(buff_out.recp,recp->name) == 0){
							printf("clinet checksum : %d & server checksum : %d\n",buff_out.checksum,checksum(buff_out.message));
							printf("No Error in CRC\n");
							printf("prv %s -> %s to %s\n", buff_out.message, cli->name,recp->name);
							send_message_private(msg, recp->uid);
						}else{
							strcpy(msg,"client not found\n");
							send_message_private(msg, cli->uid);
						}
						
						
					}
				}
				else{
					printf("ERROR CHECKSUM");
				}
				
			}
		} else if (receive == 0 || strcmp(buff_out.message, "exit") == 0){ //message
			sprintf(buff_out.message, "%s has left\n", cli->name); //message
			fprintf(logsfile,"%s has left %d-%02d-%02d %02d:%02d:%02d\n",cli->name,tm.tm_year + 1900, tm.tm_mon + 1,tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
			printf("%s", buff_out.message); //message
			send_message(buff_out.message, cli->uid);//message
			leave_flag = 1;
			
		} else {
			printf("ERROR: -1\n");
			leave_flag = 1;
		}
		
		bzero(buff_out.message, BUFFER_SZ);
		bzero(buff_out.recp, BUFFER_SZ);
		
	}

  /* Delete client from queue and yield thread */
	close(cli->sockfd);
  	queue_remove(cli->uid);
  	free(cli);
  	cli_count--;
  	update_user_list();
  	pthread_detach(pthread_self());

	return NULL;
}

void display_user_list() {
    // Display the user list on the server side
    printf("=== ACTIVE USERS ===\n");
//    system("clear");
    system("cat list.txt"); // Display the user list
}

int main(int argc, char **argv){
	if(argc != 2){
		printf("Usage: %s <port>\n", argv[0]);
		return EXIT_FAILURE;
	}
	
	//char *ip = "127.0.0.1";
	int port = atoi(argv[1]);
	int option = 1;
	int listenfd = 0, connfd = 0;
  struct sockaddr_in serv_addr;
  struct sockaddr_in cli_addr;
  pthread_t tid;

  /* Socket settings */
  listenfd = socket(AF_INET, SOCK_STREAM, 0);
  //The server creates a socket using socket function with AF_INET (IPv4) and SOCK_STREAM (TCP) protocols.
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_addr.s_addr = inet_addr(ip);
  serv_addr.sin_port = htons(port);
  

	  /* Ignore pipe signals */
	signal(SIGPIPE, SIG_IGN);
	

//	if(setsockopt(listenfd, SOL_SOCKET,(SO_REUSEPORT | SO_REUSEADDR),(char*)&option,sizeof(option)) < 0){
//		perror("ERROR: setsockopt failed");
//    return EXIT_FAILURE;
//	}

if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, (char *)&option, sizeof(option)) < 0)
    {
        perror("ERROR: setsockopt failed");
        return EXIT_FAILURE;
    }

	/* Bind */
  if(bind(listenfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
    perror("ERROR: Socket binding failed");
    return EXIT_FAILURE;
  }

  /* Listen */
  if (listen(listenfd, 10) < 0) {
    perror("ERROR: Socket listening failed");
    return EXIT_FAILURE;
	}

	printf("listen on port (127.0.0.1)\n");
	printf("=== WELCOME TO THE SERVER ===\n");
//	display_user_list();
	signal(SIGINT, catch_ctrl_c_and_exit);
	char *fName = "logs.txt";
	logsfile = fopen(fName,"a+");

	while(1){
		socklen_t clilen = sizeof(cli_addr);
		connfd = accept(listenfd, (struct sockaddr*)&cli_addr, &clilen);
		
		/* Check if max clients is reached */
		if((cli_count + 1) == MAX_CLIENTS){
			printf("Max clients reached. Rejected: ");
			print_client_addr(cli_addr);
			printf(":%d\n", cli_addr.sin_port);
			close(connfd);
			continue;
		}
		print_client_addr(cli_addr);
		/* Client settings */
		client_t *cli = (client_t *)malloc(sizeof(client_t));
		cli->address = cli_addr;
		cli->sockfd = connfd;
		cli->uid = uid++;

		/* Add client to the queue and fork thread */
		queue_add(cli);
		update_user_list();
		
        
		pthread_create(&tid, NULL, &handle_client, (void*)cli);

		/* Reduce CPU usage */
		sleep(1);
		
	}
	
	fclose(logsfile);
	return EXIT_SUCCESS;
}
