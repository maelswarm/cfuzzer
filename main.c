//
//  main.c
//  cfuzzer
//
//  Created by Brandon Barber on 11/10/15.
//  Copyright © 2015 Brandon Barber. All rights reserved.
//
/*
 TODO:
 
 FEATURES:
 Custom header ability.
 Timeout setting.
 Number of processes (1-4).
 
 -p <port #> default:1
 -o <output file> default:none
 -w <wordlist> required
 -t <timeout>
 -s <number of processes> default: 1
 -v verbose
 
 */
#include <arpa/inet.h>
#include <dirent.h>
#include <ctype.h>
#include <errno.h>
#include <fnmatch.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>

#define BACKLOG (10)
#define CHUNK_SIZE 512

FILE *addrfd = NULL; //hostname file maybe remove
FILE *relfd = NULL; //wordlist
FILE *outfd = NULL; //outfile

int recv_timeout(int s, char *reply, float timeout);

int recv_timeout(int s, char *reply, float timeout)
{
    int size_recv , total_size = 0;
    struct timeval begin , now;
    char chunk[CHUNK_SIZE];
    double timediff;
    
    //make socket non blocking
    fcntl(s, F_SETFL, O_NONBLOCK);
    
    //beginning time
    gettimeofday(&begin , NULL);
    
    while(1)
    {
        gettimeofday(&now , NULL);
        
        //time elapsed in seconds
        timediff = (now.tv_sec - begin.tv_sec) + 1e-6 * (now.tv_usec - begin.tv_usec);
        
        //if you got some data, then break after timeout
        if( total_size > 0 && timediff > timeout )
        {
            break;
        }
        
        //if you got no data at all, wait a little longer, twice the timeout
        else if( timediff > timeout*2)
        {
            break;
        }
        
        memset(chunk ,0 , CHUNK_SIZE);  //clear the variable
        if((size_recv =  (int)recv(s , chunk , CHUNK_SIZE , 0) ) <= 0)
        {
            //if nothing was received then we want to wait a little before trying again, 0.1 seconds
            usleep(100000);
        }
        else
        {
            strcpy(&reply[total_size], chunk);
            total_size += size_recv;
            //printf("%s" , chunk);
            //reset beginning time
            gettimeofday(&begin , NULL);
        }
    }
    return total_size;
}

void create_header(char *head, char *rel, const char *host) {
    //sprintf(str_header, "GET / HTTP/1.1\r\n");
    sprintf(head, "GET /%s HTTP/1.1\r\n", rel);
    sprintf(head, "%sHost: %s",head, host);
    strcat(head, "\r\nAccept: */*\r\nUser-Agent: Mozilla/5.0 (Macintosh; Intel Mac OS X 10_11_0) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/46.0.2490.80 Safari/537.36\r\nAccept-Encoding: gzip, deflate, sdch\r\nAccept-Language: en-US,en;q=0.8\r\n\r\n");
    
    //printf("\n\n%s\n\n", str_header);
}

char *parse_header(char *text) {
    
    int s = (int)(strstr(text, "\r\n\r\n")-text);
    char *str_res_header = malloc(s*sizeof(char));
    memset(str_res_header ,0 , s);
    for (int i=0; i<s; i++) {
        str_res_header[i] = text[i];
    }
    
    return str_res_header;
}

int parse_status_code(char *header) {
    char code[3];
    for (int i=0; i<3; i++) {
        code[i] = header[9+i];
    }
    
    return atoi(code);
}

void intHandler() {
    fclose(relfd);
    fclose(addrfd);
    fclose(outfd);
    exit(1);
}

int main(int argc, const char * argv[]) {
    
    signal(SIGINT, intHandler);
    int sockfd = 0;
    
    float timeout = 1.0f; //timeout
    unsigned short int port = 80; //port
    unsigned short int verbose = 0; //verbose
    char addr[1024]; //hostname
    char serv_reply[1000000];
    
    struct hostent *hp = NULL;
    struct sockaddr_in6 client_sock;
    
    if(argc<3) {
        printf("\nCommands:\n");
        printf("   -p : port number\n");
        printf("   -o : outfile\n");
        printf("   -w : wordlist\n");
        printf("   -t : timeout (default 1.0 sec)\n");
        printf("Example: ./cfuzzer google.com -p 80 -w wordLists/File_Dir_All.wordlist\n\n");
        return 1;
    }
    
    for (int i=0; i<argc; i++) {
        if (!strcmp(argv[i], "-p")) {
            if (argv[i+1] == NULL) {
                printf("\n missing port value. \n");
                return 0;
            }
            if (0>atoi(argv[i+1]) || atoi(argv[i+1])>65536) {
                printf("\n Please enter a valid port. \n");
                return 1;
            }
            port = atoi(argv[i+1]);
            i++;
        } else if (!strcmp(argv[i], "-o")) { //outfile
            outfd = fopen(argv[i+1], "w+");
            if (outfd == NULL)
            {
                printf("Error opening outfile!\n");
                exit(1);
            }
            i++;
        } else if (!strcmp(argv[i], "-w")) { //wordlist
            if (argv[i+1] == NULL) {
                printf("\n Please choose your wordlist. \n");
                return 0;
            }
            relfd = fopen(argv[i+1], "r");
            i++;
        } else if (!strcmp(argv[i], "-t")) { //timeout
            if (argv[i+1] == NULL) {
                printf("\n missing timeout value. \n");
                return 0;
            }
            if (0>atof(argv[i+1]) || atof(argv[i+1])>1000000) {
                printf("\n Please enter a valid timeout value. \n");
                return 1;
            }
            timeout = (float)atof(argv[i+1]);
            i++;
        } else if (!strcmp(argv[i], "-s")) { //number of processes
            
        } else if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help")) { //number of processes
            printf("\nCommands:\n");
            printf("\t-p : port number\n");
            printf("\t-o : outfile\n");
            printf("\t-w : wordlist\n");
            printf("\t-t : timeout (default 1.0 sec)\n");
            printf("\n\tExample : ./cfuzzer google.com -p 80 -w wordLists/File_Dir_All.wordlist\n");
        } else if (!strcmp(argv[i], "-v")) { //verbose
            verbose = 1;
        } else if (!strcmp(argv[i], "--header")) { //todo custom header
            
        } else { //address
            if(i) {
                memset((char *)&client_sock, 0, sizeof(client_sock));
                strcpy(addr, argv[i]);
                printf("\n%s\n", addr);
                for (int i=0; i<strlen(addr); i++) {
                    if (isalpha(addr[i])) { //revising
                        printf("\nNAME\n");
                        if ((hp = gethostbyname2(addr, AF_INET6)) == NULL) {
                            // get the host info
                            herror("gethostbyname");
                            return 1;
                        }
                        memcpy((char *)&client_sock.sin6_addr, hp->h_addr, hp->h_length);
                        client_sock.sin6_family = hp->h_addrtype;
                        
                        break;
                    }
                }
                
                if (hp==NULL) {
                    printf("\nNUM\n");
                    client_sock.sin6_family = AF_INET6;
                    if(inet_pton(AF_INET6, addr, &client_sock.sin6_addr)<=0)
                    {
                        printf("\n inet_pton error occured\n");
                        return 1;
                    }
                    //client_sock.sin_addr.s_addr = inet_addr(addr);
                }
            }
        }
    }
    
    if(relfd==NULL) {
        printf("\n Please choose a valid wordlist. \n");
        return 1;
    }
    
    //client_sock.sin6_len = sizeof(client_sock);
    size_t socklen = sizeof(client_sock);
    client_sock.sin6_port = htons(port);
    
    printf("\nCHECKING\n");
    
    int didnt_recieve = 0;
    char lst_rel[100];
    lst_rel[0] = '\0';
    int addr_len = (int)strlen(addr);
    
    while (!feof(relfd)) {
        
        if((sockfd = socket(AF_INET6, SOCK_STREAM, 0)) < 0)
        {
            printf("\n Error : Could not create socket %s\n", strerror(errno));
            return 1;
        }
        
        
        if(connect(sockfd, (struct sockaddr *)&client_sock, sizeof(client_sock)) < 0)
        {
            printf("\n Error : Connect Failed %s\n", strerror(errno));
            return 1;
        }
        
        while(1) {
            
            char str_rel[100];
            str_rel[0] = '\0';
            
            char str_header[1000];
            str_header[0] = '\0';
            
            memset(serv_reply ,0 , 1000000);
            
            char c;
            int i = 0;
            if (!didnt_recieve) {
                while (!feof(relfd)) {
                    c = fgetc(relfd);
                    //printf("\nREAD:%c\n",c);
                    if (c!='\n' && c!='\r' && i<99) {
                        str_rel[i++] = c;
                        str_rel[i] = '\0';
                    } else {
                        break;
                    }
                }
            } else {
                strcpy(str_rel, lst_rel);
                didnt_recieve = 0;
            }
            
            
            printf("\nChecking path: %s\n",str_rel);
            strcpy(lst_rel, str_rel);
            
            create_header(str_header, str_rel, addr);
            
            
            //Send some data
            int sret;
            if((sret=(int)send(sockfd, str_header, strlen(str_header), 0)) <= 0)
            {
                printf("\nSend failed, sret: %i\n", sret);
                break;
            }
            //printf("\n%s\n", str_header);
            
            
            //Receive a reply from the server
            int tot_size;
            if ((tot_size = recv_timeout(sockfd, serv_reply, timeout)) == 0) {
                printf("\nNothing received!\n");
                didnt_recieve=1;
                break;
            }
            //printf("\n\nrecv of size:%i\n\n", tot_size);
            
        
            char *str_res_header = parse_header(serv_reply);
            
            //printf("\nServer reply:%s\n\n%s\n",str_rel, str_res_header);
            
            int status_code = parse_status_code(str_res_header);
            
            if (status_code == 200) {
                printf("\n\nSTATUS %i\n\n", status_code);
                if (outfd != NULL) {
                    fprintf(outfd, "200 %s/%s\n", addr, str_rel);
                }
            } else if(status_code == 301) {
                printf("\n\nSTATUS %i\n\n", status_code);
                if (outfd != NULL) {
                    
                    int sn = (int)(strstr(str_res_header, "Location")-str_res_header)+10;
                    
                    char redirect[addr_len+strlen(str_rel)+100];
                    for (int i=sn; str_res_header[i]!='\r'; i++) {
                        redirect[i-sn] = str_res_header[i];
                        redirect[i-sn+1] = '\0';
                    }
                    
                    fprintf(outfd, "301 %s/%s, Location %s\n", addr, str_rel, redirect);
                }
            }
            
            free(str_res_header);
            
        }
        close(sockfd);
        printf("\nClosed sock and creating a new one.\n");
    }
    fclose(relfd);
    fclose(addrfd);
    fclose(outfd);
    printf("\nExiting\n");
    
    return 0;
}
