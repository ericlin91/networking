#include "minet_socket.h"
#include <stdlib.h>
#include <fcntl.h>
#include <ctype.h>
#include <sys/stat.h>
#include <iostream>

#define BUFSIZE 1024
#define FILENAMESIZE 100

using namespace std;

int handle_connection(int);
int writenbytes(int,char *,int);
int readnbytes(int,char *,int);

int main(int argc,char *argv[])
{
  int server_port;
  int sock,sock2;
  struct sockaddr_in sa,sa2;
  int rc,i;
  fd_set readlist;
  fd_set connections;
  int maxfd;

  /* parse command line args */
  if (argc != 3)
  {
    fprintf(stderr, "usage: http_server1 k|u port\n");
    exit(-1);
  }
  server_port = atoi(argv[2]);
  if (server_port < 1500)
  {
    fprintf(stderr,"INVALID PORT NUMBER: %d; can't be < 1500\n",server_port);
    exit(-1);
  }

  /* initialize and make socket */
  if (toupper(*(argv[1])) == 'K') {
    minet_init(MINET_KERNEL);
  } else if (toupper(*(argv[1])) == 'U') {
    minet_init(MINET_USER);
  } else {
    fprintf(stderr, "First argument must be k or u\n");
    exit(-1);
  }

  sock = minet_socket(SOCK_STREAM);
  if (sock<0) {
    fprintf(stderr, "minet_socket failed.\n");
    exit(-1);
  }
  
  /* set server address*/
  memset(&sa, 0, sizeof(sa));
  sa.sin_port=htons(server_port);
  sa.sin_family = AF_INET;
  sa.sin_addr.s_addr = INADDR_ANY;  

  /* bind listening socket */
  if(minet_bind(sock, &sa)<0){
    fprintf(stderr, "minet_bind failed.\n");
    exit(-1);
  }

  /* start listening */
  if(minet_listen(sock,3)<0){
    fprintf(stderr, "minet_listen failed.\n");
    exit(-1);
  }
//fprintf(stdout, "a\n");
  FD_ZERO(&readlist);
  FD_ZERO(&connections);
  FD_SET(sock, &connections);
  maxfd = sock;
  /* connection handling loop */
  while(1)
  {
    /* create read list */
	readlist = connections;
 // if(FD_ISSET(sock, &readlist)==0){
 //     FD_SET(sock, &readlist);
 // }

	//fprintf(stdout, "b\n");

    /* do a select */
	if(minet_select(maxfd+1, &readlist, 0, 0, 0) < 0){
		fprintf(stderr, "Select failed.\n");
	}
	//fprintf(stdout, "c\n");

    /* process sockets that are ready */
	for(i = 0; i < maxfd+1; i++){
    if(!FD_ISSET(i, &readlist))
      continue;
		
    //if(FD_ISSET(i, &readlist)){	/* for the accept socket, add accepted connection to connections */
     // fprintf(stdout, "d\n");

      		if (i == sock){
              //fprintf(stdout, "e\n");

      			sock2 = minet_accept(sock, &sa);
      			FD_SET(sock2, &connections);
      			if(sock2 > maxfd){
      				maxfd=sock2;
      			}
      		}
          else /* for a connection socket, handle the connection */
          {
            //fprintf(stdout, "f\n");
            rc = handle_connection(i);
            FD_CLR(i, &connections);
            if(i == maxfd){
              do{
                maxfd--;
              }while(FD_ISSET(maxfd, &connections));
            }
          }
      
  }//for
      	
      	
    
  }//while
}//main

int handle_connection(int sock2)
{
  char filename[FILENAMESIZE+1];
  int rc;
  int fd;
  struct stat filestat;
  char buf[BUFSIZE+1];
  char *headers;
  char *endheaders;
  char *bptr;
  int datalen=0;
  char *ok_response_f = "HTTP/1.0 200 OK\r\n"\
                      "Content-type: text/plain\r\n"\
                      "Content-length: %d \r\n\r\n";
  char ok_response[100];
  char *notok_response = "HTTP/1.0 404 FILE NOT FOUND\r\n"\
                         "Content-type: text/html\r\n\r\n"\
                         "<html><body bgColor=black text=white>\n"\
                         "<h2>404 FILE NOT FOUND</h2>\n"\
                         "</body></html>\n";
  bool ok=true;

  /* first read loop -- get request and headers*/
  memset(buf, 0, sizeof(buf));
  endheaders = "\r\n\r\n";
  string header = "";
  while((rc = minet_read(sock2, buf, sizeof(buf)))>0){
    header += (string)buf;
    if(header.find(endheaders)!=-1){
      break;
    }
  }   

 
 
  /* parse request to get file name */
  /* Assumption: this is a GET request and filename contains no spaces*/
  int start;
  int end;

  start = header.find(" ")+1;
  end = header.find(" ", start);

  if(header[start]=='/'){
    start++;
  }

  string theFile = header.substr(start, end-start);

  /* try opening the file */
  fd = open(theFile.c_str(), O_RDONLY);
  if(fd<0){
    fprintf(stderr, "File wasn't opened.\n");
    ok = false;
  } 

  /* send response */
  if (ok)
  {
    /* send headers */
    stat(theFile.c_str(), &filestat);
    sprintf(ok_response, ok_response_f, filestat.st_size);
    if(writenbytes(sock2, ok_response, strlen(ok_response))<0){
	fprintf(stderr, "Write failed.\n");
    }
    /* send file */
    if(rc = readnbytes(fd, buf, sizeof(buf)) == filestat.st_size){

	if(writenbytes(sock2, buf, sizeof(buf))<0){
		fprintf(stderr, "Write file failed.");
	}
    }
    else{
	fprintf(stderr, "Read file failed.\n");
    } 
  }
  else // send error response
  {
    writenbytes(sock2, notok_response, strlen(notok_response));
  }

  /* close socket and free space */
  close(fd);
  minet_close(sock2); 
  if (ok)
    return 0;
  else
    return -1;
}

int readnbytes(int fd,char *buf,int size)
{
  int rc = 0;
  int totalread = 0;
  while ((rc = minet_read(fd,buf+totalread,size-totalread)) > 0)
    totalread += rc;

  if (rc < 0)
  {
    return -1;
  }
  else
    return totalread;
}

int writenbytes(int fd,char *str,int size)
{
  int rc = 0;
  int totalwritten =0;
  while ((rc = minet_write(fd,str+totalwritten,size-totalwritten)) > 0)
    totalwritten += rc;

  if (rc < 0)
    return -1;
  else
    return totalwritten;
}

