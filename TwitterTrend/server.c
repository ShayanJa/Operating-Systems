/* CSci4061 F2015 Assignment 4
 * name: Ankit Nayak, Shayan Talebi
 * id: nayak015, taleb007
 */
#include <stdio.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <pthread.h>
#include "makeargv.h"


void dostuff(int sock); /* function prototype */
void error(char *msg);
void lookupTwitterDB(char cityName[], char keywords[]);
void *run(void *arg);
void printTwitterDB();
int error_message(char buf[]);


struct twitterDBEntry {
  char cityName[15];
  char keywords[200];
};

//client thread info
struct node{ 
  char cid[25];
  int socket_fd;
};


struct twitterDBEntry twitterDB[100];
int twitterDBCounter = 0;
struct node taskQueue[100];
int activeTaskCounter = 0;
int maxTasks = 0;
int currentTaskCounter = 0;
int stop = 0;

pthread_mutex_t queue_mutex;

void addToTwitterDB(struct twitterDBEntry entry)
{
  int i = 0;
  strncpy(twitterDB[twitterDBCounter].cityName, entry.cityName, strlen(entry.cityName));
  strncpy(twitterDB[twitterDBCounter++].keywords, entry.keywords, strlen(entry.keywords));
}

void lookupTwitterDB(char cityName[], char keywords[])
{
  int i = 0;
  strncpy(keywords, "NA" ,strlen("NA"));//if cant find anything it will send N/A
  for(i = 0; i < twitterDBCounter; i++) {
    if(!strncmp(twitterDB[i].cityName, cityName, strlen(cityName))) {
      strncpy(keywords, twitterDB[i].keywords,strlen(twitterDB[i].keywords));
      break;
    }
  }
  
}

void printTwitterDB()
{
  int i = 0;
  for(i = 0; i < twitterDBCounter; i++) {
    printf("CityName: %s, TrendingKeyWords: %s\n",twitterDB[i].cityName, twitterDB[i].keywords);
  }
}

void error(char *msg)
{
  perror(msg);
}

  /*  Run is the thread function that synchronizes clients
   *  Calls dostuff, our communication channel between client and server
   *  Also prints which thread is handling which client
   * 
   *  Unbounded queue implementation
   * */

void *run(void *arg)
{ 
  int counter = 0;
  int tid = *(int *)arg; 
  
  //client information
  int socketfd;
  char copy_cid[25]; //client id
  
  while(1) {
    pthread_mutex_lock(&queue_mutex);
    if (activeTaskCounter > currentTaskCounter) {
      counter = currentTaskCounter++;
      socketfd = taskQueue[counter].socket_fd; //get socketfd from queue
      bzero(copy_cid, 25);
      strcpy(copy_cid, taskQueue[counter].cid);
      pthread_mutex_unlock(&queue_mutex);


      printf ("Thread %d is handling client %s\n", tid, copy_cid); 
      dostuff(socketfd);
      printf ("Thread %d finished handling client %s\n", tid, copy_cid);
    
      pthread_mutex_lock(&queue_mutex);
      if(currentTaskCounter == activeTaskCounter) {
        activeTaskCounter = 0;
        currentTaskCounter = 0;
        
      }
      pthread_mutex_unlock(&queue_mutex);
    }
    else if (stop == 1) {
      pthread_mutex_unlock(&queue_mutex);
      break;
    }
    pthread_mutex_unlock(&queue_mutex);//prevent deadlocks
  }
  pthread_exit(NULL);
  return 0;
} 

int main(int argc, char *argv[])
{
  
  // the server stuff
  int sockfd, newsockfd, portno, clilen, pid;
  struct sockaddr_in serv_addr, cli_addr;
  int numThreads;
  numThreads = 5; // default number of threads
  if (argc < 2) {
    fprintf(stderr,"ERROR, no port provided\n");
    exit(1);
  }
  sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd < 0) 
    error("ERROR opening socket");
  if (argc == 3){//if num_threads are provided reinitiallize numThreads
    numThreads = atoi(argv[2]);
  }
  
  //Establish socket on the server end
  //Identifying network protocol and binding to socket
  bzero((char *) &serv_addr, sizeof(serv_addr));
  portno = atoi(argv[1]);
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_addr.s_addr = INADDR_ANY;
  serv_addr.sin_port = htons(portno);
  if (bind(sockfd, (struct sockaddr *) &serv_addr,
	   sizeof(serv_addr)) < 0) 
    error("ERROR on binding"); //unable to bind to socket
    
  // read from Twitter trend, populate the TwitterDB
  FILE *fp = NULL;
  char buffer_DB[120];
  struct twitterDBEntry entry;
  char *token;
  int i = 0;
  
  //pthread array and tids
  pthread_t tid[numThreads];
  int tids[numThreads];
  maxTasks = numThreads;

  // ./twitterTrend client_file num_threads
  fp = fopen("TwitterDB.txt", "r"); // Assuming that TwitterDB.txt is in the same directory as server.c
  if(fp == NULL)
    {
      printf("\nError opening file: TwitterDB.txt");
      exit(1);
    }

  //Populating TwitterDB array from TwitterDB.txt
  while (fgets (buffer_DB, sizeof(buffer_DB), fp)!=NULL) { 
    token = strtok(buffer_DB, ",");
    strncpy(entry.cityName, token, strlen(token));
    entry.cityName[strlen(token)]='\0';
    token = strtok(NULL, " ");
    strncpy(entry.keywords, token, strlen(token));
    entry.keywords[strlen(token)-1]='\0';
    addToTwitterDB(entry);
  }
  fclose(fp);
	
  // Spawn num_threads threads
  for(i = 0; i < numThreads; i++)
    {
      tids[i]=i+1;
      pthread_create(&tid[i], NULL, run, (void *) &tids[i]);
      
    }

  //wait for client connection
  listen(sockfd,numThreads);
  clilen = sizeof(cli_addr);
  printf("server listens\n");
  while (1) {
    newsockfd = accept(sockfd, 
		       (struct sockaddr *) &cli_addr, &clilen);//new client connection
    printf("server accepts connection\n");
    
    if (newsockfd < 0) {
      error("ERROR on accept");
      continue;
    }

    //add stuff to queue
    pthread_mutex_lock(&queue_mutex);
    taskQueue[activeTaskCounter].socket_fd = newsockfd;
    //printf("Adding newsockfd %d to queue\n", newsockfd);
    
    //Initiallizing Cid for the client
    bzero(taskQueue[activeTaskCounter].cid,20);
    sprintf(taskQueue[activeTaskCounter].cid,"%d.%d.%d.%d,%d",
		(int)(cli_addr.sin_addr.s_addr&0xFF),
		(int)((cli_addr.sin_addr.s_addr&0xFF00)>>8),
		(int)((cli_addr.sin_addr.s_addr&0xFF0000)>>16),
		(int)((cli_addr.sin_addr.s_addr&0xFF000000)>>24),
			cli_addr.sin_port);
    activeTaskCounter++;
    pthread_mutex_unlock(&queue_mutex);
    //~ exit(0);
    //~ }
    
  } /* end of while */
  return 0; /* we never get here */
}

/******** DOSTUFF() *********************
 This is where the magic happens. Once we 
 * have a connection to a client through
 * the socket file descripter we will 
 * perform operations on the client's request
 * These include scanning the database and
 * returning the correct 3 words for each 
 * twitterTrend request
 *****************************************/
void dostuff (int sock)
{
  int n;
  int myargc;
  char** myargv;
  char keywords[200]="\0";
  
  //Create two buffers for dual way communication
  char bufferOut[256];
  char bufferIn[256];
  char temp[20];
  int done = 0;
      
  bzero(bufferOut,256);
  //handshake 100
  printf("server sends handshaking:(100,0,)\n");
  strcpy(temp,"(100,0,)");
  strcpy(bufferOut, temp);
  
  //monitor buffer
  //printf("%s",bufferOut);
  
  n = write(sock,bufferOut,strlen(bufferOut));
  if (n < 0)
    error("ERROR writing to socket");
    
  //zero the buffers
  bzero(bufferOut,256);
  bzero(bufferIn, 256);
  
  //handling 101
  n = read(sock, bufferIn, 256);
  if(strcmp(bufferIn, "(101,0,)") == 0){
	//test print
    //printf("server recieves handshake response: %s\n",bufferIn);

  }else if(error_message(bufferIn)){
	  close(sock);
	  return;
  }else{
	//NO HANDSHAKE RESPONSE RECIEVED
	//SEND 106
	
    //printf("%s\n",bufferIn);
    printf("ERROR while reading from socket: did not recieve handshake");
    strcpy(bufferOut, "(106,68,ERROR while reading from socket: did not recieve handshake)");
	n = write(sock, bufferIn,256 );
		//printf("%d", n);
		//sleep(1);
    return;
  }
  
  bzero(bufferIn,256);
  n = read(sock,bufferIn,256);// read number of clients (extra credit)
  if(error_message(bufferIn)){
    close(sock);//106
    return;
  }
  else if (n < 0){
    error("ERROR writing to socket");
    close(sock);
  }
  int i = 0;
  int num_clients = atoi(bufferIn);
  
  bzero(bufferIn, 256);
  //printf("i: %d num_clients: %d\n",i,num_clients); 
  while(i < num_clients){
     
    bzero(bufferIn,256);
    while(!done){
		
		//read request 102
		n = read(sock,bufferIn,256);
		
		if(error_message(bufferIn)){
		   close(sock);
		   return;
		}
		//printf("Message: %s\n",bufferIn);
		myargc = makeargv((char*)bufferIn, ",", &myargv);
		if(strcmp(myargv[0], "(102") == 0){
		  int len_payload = atoi(myargv[1]);
		  char city_buf[15];
		  bzero(keywords, 200);
		  bzero(city_buf, 15);
		  
		  /* Create twitterTrend Response
		   * 103, format string
		   * */
		  
		  strncpy(city_buf, myargv[2]+1, len_payload);
		  //printf("city_buf: %s\n",city_buf);
		  lookupTwitterDB(city_buf, keywords);
		  //printf("keywords found: %s\n", keywords);
		  
		  //Processing the response
		  keywords[strlen(keywords)]='\0';
		  bzero(bufferOut,256);
		  strcpy(bufferOut, "(103,");
		  char keyword_size_buf[3];
		  bzero(keyword_size_buf,sizeof(keyword_size_buf));
		  sprintf(keyword_size_buf,"%d", (int)strlen(keywords));//converting digits to string
		  strcat(bufferOut,keyword_size_buf );
		  strcat(bufferOut,",\"");
		  strncat(bufferOut,keywords,(strlen(keywords)));
		  strcat(bufferOut,"\")");  
		  
		  printf("server sends twitterTrend response:%s\n", bufferOut);
		  //sleep(1);
		  
		  //Send 103
		  n = write(sock,bufferOut,256);
		  bzero(bufferOut, 256);
		  //Send 105 END OF RESPONSE
		  strcpy(bufferOut, "(105,0,)");
		  printf("server sends end of response:%s\n", bufferOut);
		  write(sock,bufferOut,256);
		}
		if(strcmp(myargv[0], "(104") == 0){
		  //printf("Got %s\n",bufferIn);
		  done = 1;
		  break;
		  break;
		}
		// done with one clientx.in
		bzero(bufferIn, 256);
    }//while done loop
    i++;
  }//while client extra credit loop
  
  //TEST
  //printf("Here i: %d num_clients: %d\n",i,num_clients);
  
  
  printf("server closes the connection\n");
  close(sock);
}


//ERROR MESSAGE FOR 106
int error_message(char buf[]){
	int myargc;
	char** myargv;
	myargc = makeargv((char*)buf, ",", &myargv);
	//printf("hey");
	if(strcmp(myargv[0], "(106") == 0){
		printf("%s\n", buf);
		return 1;
	}
	return 0;
}
