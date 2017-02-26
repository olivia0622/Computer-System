/* Name: Mimi Chen AndrewId: mimic1
 ******************************************************************************
 *                               DOCUMENTATION                                *
 *  SCHEMA:  Using 2 while loop. For 1st loop, when proxy reads a line from   *
 *          client, writes that line immediately to web server. For 2nd loop, *
 *          when proxy reads a response line from web server, it writes that  *
 *          back to client.                                                   *
 *  HEADER: When proxy reads header from client, proxy will update it with    *
 *          the required fields and writes it to web server.                  * 
 *  CONCURRENT: The main thread repeatedly waits for a connection request and *
 *              and then creates a peer thread to handle the request.         *
 *  CACHE:                                                                    *
 *    STRUCTURE: Cache block includes url , body, time. time is used to       *
 *               record when the content is cached. Url is the url from client*
 *               body is the content proxy read from the website.             *
 *    SCHEMA: LRU, I used timestamp to record the time the content coma into  *
 *            cache, When the cache is full I will remove the block with the  *
 *            smallest timestamp                                              *
 ******************************************************************************
 */

#include <stdio.h>
#include "csapp.h"

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

/* You won't lose style points for including this long line in your code */
static char *user_agent_hdr = "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";
static char *const_hdr = "Connection: close\r\n";
static char *const_proxy_hdr = "Proxy-Connection: close\r\n";
static sem_t mutex;
static sem_t items;
static int slot;
static int timestamp;

void* doit(void* gd);
int delete();
void writer(char *url, char *body, int time);
int search(char *url, char *body, int time);
void update_header(int fd, rio_t rio, char *h, char *f, char *p, char *url);
void parse_url(char *url, char *file, char *host, char *port);
void clienterror(int fd, char *cause, char *errnum, 
		 char *shortmsg, char *longmsg);
struct cache_t {
	int time;
	char url[MAXLINE];
	char body[MAX_OBJECT_SIZE];
};
struct cache_t cache_blocks[10];


int main(int argc, char **argv) {
	int listenfd, *connfd;
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;
    char port[MAXLINE], host[MAXLINE];
    pthread_t tid;
    /* Check command line args */
    
    Signal(SIGPIPE, SIG_IGN);

    int i = 0;
    for(i = 0; i < 10; i++) {
	    cache_blocks[i].url[0] = '\0';
		cache_blocks[i].body[0] ='\0';
		cache_blocks[i].time = 0;  
    }
    Sem_init(&mutex,0,1);
    Sem_init(&items,0,1);
    slot = 0;
    if (argc != 2) {
	    fprintf(stderr, "usage: %s <port>\n", argv[0]);
	    exit(1);
    }

    listenfd = Open_listenfd(argv[1]);
    while (1) {
		clientlen = sizeof(clientaddr);
		connfd = Malloc(sizeof(int));
		*connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen); 
	    Getnameinfo((SA *) &clientaddr, clientlen, host, MAXLINE, 
	                    port, MAXLINE, 0);
	    Pthread_create(&tid, NULL, doit, connfd);                                         
    }
    return 0;
}
/* $begin doit
 * Do the connection to client and read the content and 
 * write to the server.
 */
void* doit(void* gd) {
	int fd = *((int *)gd);
	char body[MAX_OBJECT_SIZE];
	P(&mutex);
	timestamp++;
	V(&mutex);
    Pthread_detach(pthread_self()); 
    Free(gd);     
    int n;
    char buf[MAXLINE],buf1[MAXLINE], method[MAXLINE], url[MAXLINE];
    char file[MAXLINE], host[MAXLINE], version[MAXLINE], port[MAXLINE];
    rio_t rio;
    rio_t rio1;
    strcpy(port, "80");
    /* Read request line and headers */
    Rio_readinitb(&rio, fd);
    if (!Rio_readlineb(&rio, buf, MAXLINE)) 
        return NULL;
    sscanf(buf, "%s %s %s", method, url, version);    
    if (strcasecmp(method, "GET")) {                     
        clienterror(fd, method, "501", "Not Implemented",
                    "Proxy does not implement this method");
        return NULL;
    }
    int flag = search(url, body, timestamp);
    if (flag != 0) {
    	Rio_writen(fd, body, strlen(body));
    	Close(fd);
    	return NULL;
    }                                                  
    /* Parse URI from GET request */
    parse_url(url, file, host, port);   
    int write_clientfd = Open_clientfd(host, port);
    if (write_clientfd == -1) {
    	clienterror(fd, file, "400", "Bad Request",
		        "Proxy can not open the connection");
	    return NULL;
    }
    //Write the new header
    update_header(write_clientfd, rio, host, file, port, url);
    Rio_readinitb(&rio1, write_clientfd); /* Initialize RIO read structure */

    /* Read line by line from client & echo back 
       NOTE: Rio_readlineb returns the number of bytes read */
    while((n = Rio_readlineb(&rio1, buf1, MAXLINE)) > 0) {
        Rio_writen(fd, buf1, n); /* Write message back to client */
        strcat(body, buf1);
    }
    writer(url, body, timestamp);
    Close(write_clientfd);
    Close(fd);
    return NULL;
}
//Update the header to what we need
void update_header(int fd, rio_t rio, char *host, char *file,
	char *port, char *url) 
{
	char buf[MAXLINE];
	sprintf(buf, "GET %s HTTP 1.0\r\n", file);
        Rio_writen(fd, buf, strlen(buf));
	while(strcmp(buf, "\r\n")){
		Rio_readlineb(&rio, buf, MAXLINE);
		if (strstr(buf, "Connection:")) {
			strcpy(buf, const_hdr);
			Rio_writen(fd,buf,strlen(buf));
			strcpy(buf, const_proxy_hdr);
		}
		if (strstr(buf, "User-Agent:")) {
			strcpy(buf, user_agent_hdr);
		}
		Rio_writen(fd,buf,strlen(buf));
	}
	Rio_writen(fd,"\r\n",strlen("\r\n"));
}

//Parse the url
void parse_url(char *url, char *file, char *host, char *port) {
	char tmp[MAXLINE];
	sscanf(url, "%*[^://]://%[^/]%s", host, file);
	if (strstr(host,":") != NULL) {
        strcpy(tmp, host);
        sscanf(tmp, "%[^:]:%s",host, port);	
    }
}
/*
 * clienterror - returns an error message to the client
 */
/* $begin clienterror */
void clienterror(int fd, char *cause, char *errnum, 
		 char *shortmsg, char *longmsg) 
{
    char buf[MAXLINE], body[MAXBUF];

    /* Build the HTTP response body */
    sprintf(body, "<html><title>Tiny Error</title>");
    sprintf(body, "%s<body bgcolor=""ffffff"">\r\n", body);
    sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
    sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
    sprintf(body, "%s<hr><em>The Tiny Web server</em>\r\n", body);

    /* Print the HTTP response */
    sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-type: text/html\r\n");
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));
    Rio_writen(fd, buf, strlen(buf));
    Rio_writen(fd, body, strlen(body));
}
/* $end clienterror */
//Search for the cache hit block.
int search(char *url, char *body, int time) {
	P(&items);
        slot++;
        if (slot == 1) {
            P(&mutex);
        }
        V(&items);
	int flag = 0;
	int i = 0;
	for (i = 0; i < 10; i++) {
		if (strcmp(cache_blocks[i].url, url) == 0) {
		    strcpy(body, cache_blocks[i].body);
		    cache_blocks[i].time = time;
		    flag = 1;
		}
	}
	P(&items);
        slot--;
        if (slot == 0) {
            V(&mutex);
        }
	V(&items);
        return flag;
}
//Write the content to cache
void writer(char *url, char *body, int time) {
	P(&mutex);
	if (strlen(body) < MAX_OBJECT_SIZE) {
		int i = 0;
		int flag = 1;
		for (i = 0; i < 10; i++) {
			if (cache_blocks[i].url[0] == '\0') {
		            strcpy(cache_blocks[i].url, url);
			    strcpy(cache_blocks[i].body, body);
			    cache_blocks[i].time = time;
			    flag = 0;
			    break;
			}
		}
		if (flag) {
			int j = delete();
			strcpy(cache_blocks[j].url, url);
			strcpy(cache_blocks[j].body, body);
			cache_blocks[j].time = time;
		}
	}
	V(&mutex);
}
//Delete the block when the block if cache is full.
int delete() {
	int i = 0;
	int latest = cache_blocks[0].time;
	int res = 0;
	for (i = 0; i < 10; i++) {
		if (cache_blocks[i].time < latest) {
			res = i;
		}
	}
	cache_blocks[res].url[0] = '\0';
	cache_blocks[res].body[0] = '\0';
	cache_blocks[res].time = 0;
	return res;
}

