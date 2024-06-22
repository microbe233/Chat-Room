#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>


//#define LENGTH 2048
#define LENGTH 6068
#define CRCNUM 200

// Global variables
volatile sig_atomic_t flag = 0;
int sockfd = 0;
char name[32];
FILE *fileName;

//struct of message
struct message_struct
{
	char recp[LENGTH];
    char message[LENGTH];
    unsigned int checksum;
    char CRCMessage[LENGTH+32];
} msg;


void str_overwrite_stdout() {
  printf("%s", "> ");
  fflush(stdout);
}

void str_overwrite_stdoutRcv() {
  printf("%s", "> private msg- client name you want to send:  ");
  fflush(stdout);
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

void catch_ctrl_c_and_exit(int sig) {
    flag = 1;
}

//checksum
unsigned char checksum(char *string) {
  unsigned char sum = 0;
  for (int i = 0; i < strlen(string); i++) {
    sum += string[i];
  }
  return sum % 256;
}

// reverse string

void reverseString(char str[]) {
    int length = strlen(str);
    int start = 0;
    int end = length - 1;

    while (start < end) {
        // Swap characters at start and end indices
        char temp = str[start];
        str[start] = str[end];
        str[end] = temp;

        // Move indices towards the center
        start++;
        end--;
    }
}

// CRC Calc
char *crc(char *message)
{
    	// Calculate the CRC checksum here
	unsigned char sum = 0;
  	for (int i = 0; i < strlen(message); i++) {
    		sum += message[i];
  	}
  	sum = sum % CRCNUM;
	char *crcmsg = malloc(LENGTH+4);
	
	sprintf(crcmsg,"%s%c",message,(char)sum);
	
	
	return crcmsg;
}

void send_msg_handler() {
  	char message[LENGTH] = {};
	char buffer[LENGTH] = {};
	char recp[LENGTH] = {};
	
  while(1) {
  	str_overwrite_stdout();
  	printf("write for whom you want the message: ");
    	fgets(recp, LENGTH, stdin);
    	str_trim_lf(recp, LENGTH);
    	
    str_overwrite_stdout();
    printf("Enter message: ");
    	fgets(message, LENGTH, stdin);
    	str_trim_lf(message, LENGTH);
    	

    	if (strcmp(message, "exit") == 0) {
    		
			fclose(fileName);
			break;
    	} else {
    		
//    		char response[32];
//            str_overwrite_stdout();
//            printf("Do you want to corrupt the message? (yes/no): ");
//            fgets(response, sizeof(response), stdin);
//            str_trim_lf(response, sizeof(response));
//
//            if (strcmp(response, "yes") == 0) {
//            	
//                reverseString(message);
//            }
            
            // Display the user list on the client side
//    system("clear"); // Clear the terminal for better visibility
    printf("=== ACTIVE USERS ===\n");
    system("cat list.txt"); // Display the user list
            
      
      
      sprintf(buffer , "%s",message);
      
      fprintf(fileName,"%s : %s\n",name,message);
      strcpy(msg.message,buffer);
      
      
      strcpy(msg.recp,recp);
      
      msg.checksum = (unsigned int)checksum(buffer);
      
      strcpy(msg.CRCMessage,crc(buffer));

      send(sockfd , &msg , sizeof(msg) , 0);
      
    }

    bzero(message, LENGTH);
    bzero(buffer, LENGTH);
  }
  catch_ctrl_c_and_exit(2);
}

void recv_msg_handler() {
	char message[LENGTH] = {};
	 
  while (1) {
    int receive = recv(sockfd, message, LENGTH, 0);
    //checksum for recv meg
    
    if (receive > 0) {
      printf("\n%s\n", message);
      fprintf(fileName,"%s\n",message);
      str_overwrite_stdoutRcv();
    } else if (receive == 0) {
			break;
    } else {
			// -1
		}
		memset(message, 0, sizeof(message));
  }
}

int main(int argc, char **argv){
	if(argc != 2){
		printf("Usage: %s <port>\n", argv[0]);
		return EXIT_FAILURE;
	}

	char *ip = "127.0.0.1";
	int port = atoi(argv[1]);

	signal(SIGINT, catch_ctrl_c_and_exit);

	printf("Please enter your name: ");
  	fgets(name, 32, stdin);
  	str_trim_lf(name, strlen(name));


	if (strlen(name) > 32 || strlen(name) < 2){
		printf("Name must be less than 30 and more than 2 characters.\n");
		return EXIT_FAILURE;
	}

	struct sockaddr_in server_addr;

	/* Socket settings */
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
  	server_addr.sin_family = AF_INET;
  	server_addr.sin_addr.s_addr = inet_addr(ip);
  	server_addr.sin_port = htons(port);


  // Connect to Server
  int err = connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr));
  if (err == -1) {
		printf("ERROR: connect\n");
		return EXIT_FAILURE;
	}

	// Send name
	send(sockfd, name, 32, 0);
	
	printf("Connected to server succusfully!\n");
	printf("=== WELCOME TO THE CHATROOM ===\n");
	printf("=== ACTIVE USERS ===\n");
    system("cat list.txt"); // Display the user list
	
	time_t t = time(NULL);
  	char fName[200];
	struct tm tm = *localtime(&t);
	
  	sprintf(fName,"%d-%02d-%02d %02d:%02d:%02d %s.txt", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec,name);
	
	fileName = fopen(fName,"w");

	pthread_t send_msg_thread;
  if(pthread_create(&send_msg_thread, NULL, (void *) send_msg_handler, NULL) != 0){
		printf("ERROR: pthread\n");
    return EXIT_FAILURE;
	}

	pthread_t recv_msg_thread;
  if(pthread_create(&recv_msg_thread, NULL, (void *) recv_msg_handler, NULL) != 0){
		printf("ERROR: pthread\n");
		return EXIT_FAILURE;
	}

	while (1){
		if(flag){
			printf("\nBye\n");
			fclose(fileName);
			break;
    }
	}

	close(sockfd);

	return EXIT_SUCCESS;
}
