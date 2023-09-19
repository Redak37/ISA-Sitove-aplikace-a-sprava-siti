/*
 *  File:       isaserver.hpp
 *  Solution:   ISA - HTTP nastenka
 *  Author:     Radek Duchon - xducho07, VUT FIT 3BIT 2019/20
 *  Compiled:   g++ 7.4.0
 *  Date:       17.11.2019
 */


#ifndef __ISASERVER_
#define __ISASERVER_

//print help
void help(void);

//search for argument to print help
int arghelp(int argc, char *argv[]);

//receiving of HTTP message
//first get full header of HTTP msg, then try
//to get body, if content-length is defined.
std::string receive(int socket);

//return response to client based on request and actual state of boards
std::string response(std::string msg);

//run server at specified port
void server(int port);

#endif
