/* A simple server in the internet domain using TCP
   The port number is passed as an argument 
   This version runs forever, forking off a separate 
   process for each connection
*/
#include <stdio.h>
#include <sys/types.h>   // definitions of a number of data types used in socket.h and netinet/in.h
#include <sys/socket.h>  // definitions of structures needed for sockets, e.g. sockaddr
#include <netinet/in.h>  // constants and structures needed for internet domain addresses, e.g. sockaddr_in
#include <stdlib.h>
#include <strings.h>
#include <sys/wait.h>	/* for the waitpid() system call */
#include <signal.h>	/* signal name macros, and the kill() prototype */
#include <fcntl.h>
#include <sys/stat.h>
#include <time.h>

void sigchld_handler(int s)
{
    while(waitpid(-1, NULL, WNOHANG) > 0);
}

void dostuff(int); /* function prototype */
void error(char *msg)
{
    perror(msg);
    exit(1);
}

// contains all information needed in 
// an HTTP response message
typedef struct http_msg
{
	char* method;
	char* url;
	char* version;
	char* connection;
} http_msg_t;

// parse the first field in 'msg' into 'field'
// different fields in msg should be separated by space or newline
char* parse_field(char* msg, char*field)
{
	for(;;){
		if(*msg == ' ' || *msg == '\n' || *msg == '\r') {
			msg++;
			continue;
		}
		else	break;
	}
	for(;;){
		if(*msg == ' ' || *msg == '\n' || *msg == 0 || *msg == '\r') break;
		*field = *msg;
		msg++;
		field++;
	}
	*field = 0;
	return msg;
}

http_msg_t* parse_msg(char* msg)
{
	http_msg_t *http_msg = malloc(sizeof(http_msg_t));
	http_msg->method = malloc(10*sizeof(char));
	http_msg->url = malloc(1024*sizeof(char));
	http_msg->version = malloc(10*sizeof(char));
	http_msg->connection = malloc(20*sizeof(char));

	msg = parse_field(msg, http_msg->method);
	msg = parse_field(msg, http_msg->url);

	char* newurl = malloc(20);
	sprintf(newurl, ".%s", http_msg->url);
	http_msg->url = newurl;
	msg = parse_field(msg, http_msg->version);
	msg = strstr(msg, "Connection:");

	if(!msg)	error("ERROR illegal message");

	msg = parse_field(msg+11, http_msg->connection);
	
	return http_msg;
}

char* content_type(char* filename)
{
	char* ftype = malloc(10);
	bzero(ftype, 10);
	if(strstr(filename, ".html") || 
		strstr(filename, ".txt"))
		strcpy(ftype, "text/html");
	else if(strstr(filename, ".jpeg"))
		strcpy(ftype, "image/jpeg");
	else if(strstr(filename, ".gif"))
		strcpy(ftype, "image/gif");
	return ftype;
}

char* format_response(http_msg_t* msg)
{
	char *response = malloc(1024*sizeof(char));
	bzero(response, 1024);

	if(strcmp(msg->method, "GET")){
		sprintf(response, "%s 400 Bad Request\n\n", msg->version);
		return response;
	}

	struct stat fstat;
	if(stat(msg->url, &fstat)<0){
		sprintf(response, "%s 404 Not Found\n\n", msg->version);
		return response;
	}
	char *line = malloc(256*sizeof(char));
	int n = sprintf(response, "%s, 200 OK\n", msg->version);

	//printf("sprintf returns: %d\n", n);
	sprintf(line, "Connection: %s\n", msg->connection);
	response = strcat(response, line);
	bzero(line,256);

	time_t tt;
	time(&tt);
	sprintf(line, "Date: %sServer: My WebServer\n", ctime(&tt));
	response = strcat(response, line);
	bzero(line,256);

	sprintf(line, "Last-Modified: %s", ctime(&fstat.st_mtime));
	response = strcat(response, line);
	bzero(line,256);

	sprintf(line, "Content-Length: %d\n", fstat.st_size);
	response = strcat(response, line);
	bzero(line,256);

	char* ftype = content_type(msg->url);
	sprintf(line, "Content-Type: %s\n\n", ftype);
	response = strcat(response, line);
	bzero(line,256);

	return response;
}


int main(int argc, char *argv[])
{
     int sockfd, newsockfd, portno, pid;
     socklen_t clilen;
     struct sockaddr_in serv_addr, cli_addr;
     struct sigaction sa;          // for signal SIGCHLD

     if (argc < 2) {
         fprintf(stderr,"ERROR, no port provided\n");
         exit(1);
     }
     sockfd = socket(AF_INET, SOCK_STREAM, 0);
     if (sockfd < 0) 
        error("ERROR opening socket");
     bzero((char *) &serv_addr, sizeof(serv_addr));
     portno = atoi(argv[1]);
     serv_addr.sin_family = AF_INET;
     serv_addr.sin_addr.s_addr = INADDR_ANY;
     serv_addr.sin_port = htons(portno);
     
     if (bind(sockfd, (struct sockaddr *) &serv_addr,
              sizeof(serv_addr)) < 0) 
              error("ERROR on binding");
     
     listen(sockfd,5);
     
     clilen = sizeof(cli_addr);
     
     /****** Kill Zombie Processes ******/
     sa.sa_handler = sigchld_handler; // reap all dead processes
     sigemptyset(&sa.sa_mask);
     sa.sa_flags = SA_RESTART;
     if (sigaction(SIGCHLD, &sa, NULL) == -1) {
         perror("sigaction");
         exit(1);
     }
     /*********************************/
     
     while (1) {
         newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);
         
         if (newsockfd < 0) 
             error("ERROR on accept");
         
         pid = fork(); //create a new process
         if (pid < 0)
             error("ERROR on fork");
         
         if (pid == 0)  { // fork() returns a value of 0 to the child process
             close(sockfd);
             dostuff(newsockfd);
             exit(0);
         }
         else //returns the process ID of the child process to the parent
             close(newsockfd); // parent doesn't need this 
     } /* end of while */
     return 0; /* we never get here */
}

/******** DOSTUFF() *********************
 There is a separate instance of this function 
 for each connection.  It handles all communication
 once a connnection has been established.
 *****************************************/
void dostuff (int sock)
{
   int n, nr, nw;
   char buffer[1024];
      
	bzero(buffer,1024);

	//read request message from client:
   	n = read(sock,buffer,1024);
	if (n < 0) error("ERROR reading from socket");
	else if(n == 1024)
		error("ERROR message too long");

	printf("The message from client: \n%s\n",buffer);

	// parse the client message into a http_msg_t struct
	http_msg_t* http_msg = parse_msg(buffer);
	bzero(buffer, 1024);

	// format the response message
	char *response = format_response(http_msg);
	printf("The response from server: \n%s\n", response);
	bzero(response, 1024);
	
	// write the response message to client's socket
	n = write(sock,response,strlen(response));
	if (n < 0) error("ERROR writing to socket");

	// open the requested file
	int fd = open(http_msg->url, O_RDONLY);
	if(fd < 0) error("ERROR reading file");

	// send the data to client
	while(1)
	{	
		//read file into buffer
		nr = read(fd, response, 1023);

		if (nr <= 0) { break;}

		//write data from buffer to client
		nw = write(sock, response, nr);

		bzero(response,1024);
	}

	close(fd);
	free(response);
}


