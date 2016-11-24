/* CSci4061 F2015 Assignment 4
 * name: Ankit Nayak, Shayan Talebi
 * id: nayak015, taleb007
 */

#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>//something only client.c file
#include <string.h>
#include <strings.h>
#include <stdlib.h> 
#include "makeargv.h"

void error(char *msg)
{
  perror(msg);
  exit(0);
}

//user defined error message for 106 calls
int error_message(char buf[]){
	int myargc;
	char** myargv;
	myargc = makeargv((char*)buf, ",", &myargv);
	//printf("hey");
	if(strcmp(myargv[0], "(106") == 0){
		char err[256];
		bzero(err,256);
		strncpy(err, myargv[2], strlen(myargv[2])-1);
		printf("%s\n", err);
		return 1;
	}
	return 0;
}


int main(int argc, char *argv[]){
  int sockfd, portno, n;// the names are self explanatory. n is a check variable for read/write
  struct sockaddr_in serv_addr;//server address to which we will connect
  struct hostent *server;// host entry
  char keywords[200]="\0"; //store keywords received from the server
  char bufferOut[256]; // buffers to receive message from server
  char bufferIn[256]; // buffers to send message to the server
  if (argc < 3) { // error handle
    fprintf(stderr,"usage %s hostname port\n", argv[0]);
    exit(0);
  }
  portno = atoi(argv[2]);//same as the server.c, gets the port number
  sockfd = socket(AF_INET, SOCK_STREAM, 0);//create socket
  if (sockfd < 0) 
    error("ERROR opening socket");
  server = gethostbyname(argv[1]);//get the IP from DNS
  if (server == NULL) {// in case it cannot find the domain name
    fprintf(stderr,"ERROR, no such host\n");
    exit(0);
  }
  bzero((char *) &serv_addr, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  bcopy((char *)server->h_addr, 
  	(char *)&serv_addr.sin_addr.s_addr,
  	server->h_length);//good way to copy 2 strings in byte sequence
  serv_addr.sin_port = htons(portno);//this has to be done before the connect()
  if (connect(sockfd,(struct sockaddr *)&serv_addr,sizeof(serv_addr)) < 0) 
    error("ERROR connecting");
  printf("client connects\n");

  bzero(bufferOut,256);
  bzero(bufferIn, 256);
  char temp[20];

  //wait for handshake 100
  n = read(sockfd, bufferOut, 255);
  if (n < 0) 
    error("ERROR writing to socket");
  if(strcmp(bufferOut, "(100,0,)")==0){
    //printf("client recieves handshake: %s\n", bufferOut);
  } else if(error_message(bufferOut)){
	  exit(0);
  } else{
    error("no handshake");
    //exit(0);
  }

  //send handshake response 101
  bzero(bufferOut,256);
  printf("client sends handshake response: (101,0,)\n");
  strcpy(temp, "(101,0,)");
  bzero(bufferIn,256);
  strcpy(bufferIn, temp); 
  
  n = write(sockfd, bufferIn, 256);
  if (n < 0) 
    error("ERROR writing to socket");

  //process twitter trend request 
  int i = 3;
  FILE* fp_client;
  FILE* fp_result;
  int ar = argc -i;
  sprintf(bufferIn, "%d", ar);
  //printf("sending for extra credit: %s\n",bufferIn);
  n = write(sockfd, bufferIn, 256);
  
  int myargc;
  char ** myargv;
  int len_payload_r;
  while(i < argc){ 
	//EC: extra credit implementation
	//run as many clientX.in files as you want
    bzero(bufferIn,256);
    //function fgets clientX.in 
    int num_cities = 0;
	//check if argv[i] argument is a valid file
    if((fp_client = fopen(argv[i], "r")) == NULL){
		strcpy(bufferIn, "(106,23,invalid clientX.in file)");
		n = write(sockfd, bufferIn,256 );
		//sleep(1); //to slow down
		error("invalid clientX.in file");
	}
	//fp_client = fopen(argv[i], "r");
    
    //make result file and open it in the same path
    char result_name[256];
    bzero(result_name, 256);
    strcpy(result_name, argv[i]);
    strcat(result_name, ".result");
    //printf("name: %s\n", result_name);
    fp_result = fopen(result_name, "w");
    //printf("client: %s\n",argv[i]);
    char buf_city[256];
    bzero(buf_city, 256);
    bzero(bufferIn,256);
    
    while(fgets(buf_city, 256, fp_client) != NULL){
	  //sleep(1);
	  
	  //sending request 102
      int len_payload = strlen(buf_city)-1;
      strcpy(bufferIn, "(102,"); 
      char citysize_buf[3];
      bzero(citysize_buf,sizeof(citysize_buf));
      sprintf(citysize_buf,"%d", len_payload);//converting digits to string
      strcat(bufferIn,citysize_buf );
      strcat(bufferIn,",\"");
      strncat(bufferIn,buf_city,(strlen(buf_city)-1));
      strcat(bufferIn,"\")");
		
	  //bufferIn is now in format (102,size, "city")	
      printf("client sends twitterTrend request:%s\n", bufferIn);
      n = write(sockfd,bufferIn,strlen(bufferIn));//send to server request
      if (n < 0) {
        error("ERROR writing to socket");
	  }
      bzero(bufferIn, 256);

      //written the city name
      //wait for response 103
      n = read(sockfd, bufferOut, 256);
      if (n < 0)
        error("ERROR writing to socket");
      else if(error_message(bufferOut))
		exit(0);
      //extract the message code
      
      
      //printf("Client read: %s\n", bufferOut); //buffer check
      //makeargv 
      myargc = 0;
      myargc = makeargv((char*)bufferOut, ",", &myargv);                                                                                                                                                                                       
   
      //printf("myargv[0]: %s\n",myargv[0]);
      //Check for 103 response
      if(strcmp(myargv[0], "(103")==0){
		myargc = makeargv((char*)bufferOut, "\"", &myargv);
		//printf("client recieves three names: %s\n", myargv[1]);
        //write to clientx.in.result file Format- Minneapolis : UMN,Lakes,Snow
		//printf("Here I will write to result file. Function to be implementated.\n");
		//write to result file
		int n;
		char city_name[256];
		bzero(city_name,256);
		
		//write response of twitter trend response
		strncpy(city_name, buf_city, len_payload);
		n = fwrite(city_name, sizeof(char), strlen(city_name), fp_result);
		n = fwrite(" : ", sizeof(char), 3, fp_result);
		n = fwrite(myargv[1], sizeof(char), strlen(myargv[1]), fp_result);
		n = fwrite("\n", 1,1,fp_result);
		bzero(buf_city,256);
		
		bzero(bufferOut, 256);
			 //wait for response 105
	    n = read(sockfd, bufferOut, 256);
	    //printf("at the end client read: %s\n",bufferOut);
	    if(strcmp(bufferOut, "(105,0,)")==0){
		  //test message
		  //printf("client recieves end of response\n");
	    }
	    else{
			bzero(bufferIn,256);
			strcpy(bufferIn, "(106,0,no end of response received)");
			n = write(sockfd, bufferIn,256 );
			error("no end of response received");
		}
		
	  }
     
    }//while loop for file contents

	//close file pointers
    fclose(fp_client);
    fclose(fp_result);
    i++;
  }//while loop for clientx.in 
  
  //client sends end of request 104                                                                                                                                                                       
  bzero(bufferIn, 256);
  bzero(bufferOut, 256);
  strcpy(bufferIn, "(104,0,)");
  printf("client sends end of request: %s\n", bufferIn);
  n = write(sockfd,bufferIn,strlen(bufferIn));
  bzero(bufferIn,256);
  bzero(bufferOut, 256);
  
  //end connection and close socket
  //request complete
  printf("client closes connection\n");
  close(sockfd);
  return 0;
}

