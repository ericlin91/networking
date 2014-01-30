#include "minet_socket.h"
#include <stdlib.h>
#include <ctype.h>
#include <iostream>

using namespace std;

#define BUFSIZE 1024

int write_n_bytes(int fd, char * buf, int count);

int main(int argc, char * argv[]) {
    char * server_name = NULL;
    int server_port = 0;
    char * server_path = NULL;

    int sock = 0;
    int rc = -1;
    int datalen = 0;
    bool ok = true;
    struct sockaddr_in sa;
    FILE * wheretoprint = stdout;
    struct hostent * site = NULL;
    char * req = NULL;

    char buf[BUFSIZE + 1];
    char * bptr = NULL;
    char * bptr2 = NULL;
    char * endheaders = NULL;
   
    struct timeval timeout;
    fd_set set;

    /*parse args */
    if (argc != 5) {
    fprintf(stderr, "usage: http_client k|u server port path\n");
    exit(-1);
    }

    server_name = argv[2];
    server_port = atoi(argv[3]);
    server_path = argv[4];



    /* initialize minet */
    if (toupper(*(argv[1])) == 'K') { 
    minet_init(MINET_KERNEL);
    } else if (toupper(*(argv[1])) == 'U') { 
    minet_init(MINET_USER);
    } else {
    fprintf(stderr, "First argument must be k or u\n");
    exit(-1);
    }

    /* create socket */
    sock = minet_socket(SOCK_STREAM);
    if (sock<0) {
        fprintf(stderr, "minet_socket failed.\n");
        exit(-1);
    }

    // Do DNS lookup
    /* Hint: use gethostbyname() */
    site = gethostbyname(server_name);
    if(site==NULL){
        fprintf(stderr, "gethostbyname failed.\n");
        exit(-1);
    }   

    /* set address */
    memset(&sa, 0, sizeof(sa));
    sa.sin_port=htons(server_port);
    memcpy(&(sa.sin_addr.s_addr), site->h_addr_list[0], site->h_length);
    sa.sin_family=AF_INET;

    /* connect socket */
    if(minet_connect(sock, (struct sockaddr_in *)&sa)<0){
        fprintf(stderr, "minet_connect failed.\n");
        exit(-1);
    }
    

    /* send request */
    char* reqform = "GET /%s HTTP/1.0\r\nHost: %s\r\nUser-agent: Mozilla/4.0\r\nConnection: close\r\n\r\n";
    req = (char*)malloc(strlen(reqform)+strlen(server_path)+strlen(server_name)+1); //add 1 for terminating \0
    sprintf(req, reqform, server_path, server_name);
    if(minet_write(sock, req, strlen(req))<0){
        fprintf(stderr, "minet_write failed.\n");
        exit(-1);
    }

    /* wait till socket can be read */
    /* Hint: use select(), and ignore timeout for now. */
    FD_ZERO(&set);
    FD_SET(sock, &set);
    if(minet_select(sock+1, &set, 0, 0, &timeout)<0){
        fprintf(stderr, "minet_write failed.\n");
        exit(-1);
    }

    /* first read loop -- read headers */
    memset(buf, 0, sizeof(buf));
    endheaders = "\r\n\r\n";
    string header = "";
    while(minet_read(sock, buf, sizeof(buf))>0){
        header += (string)buf;
        if(header.find(endheaders)!=-1){
            break;
        }
    }

    /* examine return code */   
    //Skip "HTTP/1.0"
    //remove the '\0'
    // Normal reply has return code 200
    int rcode_start = header.find(" ")+1;
    string rcode = header.substr(rcode_start, 3);
    if(rcode.compare("200")==0){
	ok = true;
    }
    else{
	ok = false;
    }


    /* print first part of response */
    if(ok==true){
    	cout << header;
    	fflush(stdout);
    }
    else{
	cerr << header;
	fflush(stderr);
    }  

    /* second read loop -- print out the rest of the response */
    while(minet_read(sock, buf, sizeof(buf))>0){
    if(ok==true){
    	cout << buf;
    	fflush(stdout);
    }
    else{
	cerr << buf;
	fflush(stderr);
    }  
}

    /*close socket and deinitialize */
    minet_close(sock);
    minet_deinit();

    if (ok) {
    return 0;
    } else {
    return -1;
    }
}

int write_n_bytes(int fd, char * buf, int count) {
    int rc = 0;
    int totalwritten = 0;

    while ((rc = minet_write(fd, buf + totalwritten, count - totalwritten)) > 0) {
    totalwritten += rc;
    }
    cout << "done\n";
    if (rc < 0) {
    return -1;
    } else {
    return totalwritten;
    }
}


