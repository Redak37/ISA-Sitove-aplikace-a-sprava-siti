/*
 *  File:       isaclient.hpp
 *  Solution:   ISA - HTTP nastenka
 *  Author:     Radek Duchon - xducho07, VUT FIT 3BIT 2019/20
 *  Compiled:   g++ 7.4.0
 *  Date:       17.11.2019
 */


#ifndef __ISACLIENT_
#define __ISACLIENT_

//return ip of host
char *HostToIp(char *host);

//print help
void help(void);

//search for argument to print help
int arghelp(int argc, char *argv[]);

//receiving of HTTP message
//first get full header of HTTP msg, then try
//to get body, if content-length is defined.
std::string receive(int socket);

//print HTTP header to stderr and HTTP body to stdout
int process(std::string msg);

//return predefined message from arguments, potencionally dangerous, need to control args before calling of this function
std::string msg_create(char *host, char *args[]);

//create socket, connect to a server, send message and print response
int client(char *host, int port, char *args[]);

#endif
