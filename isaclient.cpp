/*
 *  File:       isaclient.cpp
 *  Solution:   ISA - HTTP nastenka
 *  Author:     Radek Duchon - xducho07, VUT FIT 3BIT 2019/20
 *  Compiled:   g++ 7.4.0
 *  Date:       17.11.2019
 */

#include <iostream>
#include <unistd.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <regex>
#include "isaclient.hpp"

#define BUFFER 16384

//print error message and help, then return 1
#define handle_error(msg) \
    do {\
        cerr << msg;\
        help();\
        return 1;\
    } while (0)

//call perror for predefined errors and print help, then return 1
#define perr(msg) \
    do {\
        perror(msg);\
        help();\
        return 1;\
    } while (0)


using namespace std;

//return ip of host
char *HostToIp(char *host)
{
    if (hostent *hostname = gethostbyname(host))
        return inet_ntoa(**(in_addr**)hostname->h_addr_list);
    return NULL;
}

//print help
void help(void)
{
    cout << "starting the program:\n\t./isaclient -H <host> -p <port> <command>\n\
        <host> is address of server (can be written like IP adress or like domain name)\n\
        <port> is number from 0 to 65535, alltrough ports 0-1023 are restricted just for root.\n\
        <command> stands for \"boards\", \"board\" or \"list\", these commands have their own arguments:\n\
            boards: without other arguments\n\
                send request to get list of boards\n\
            board: takes as argument add|delete|list <name>\n\
                board add <name>\n\
                    sends request to add board <name> on server\n\
                board delete <name>\n\
                    sends request to delete board <name> on server\n\
                board list <name>\n\
                    sends request to get content od board <name> from server\n\
            item: takes as argument add|delete|update <name> with additionally <id>, <content> or both\n\
                item add <name> <content>\n\
                    sends request to add message with <content> to a board <name> on server\n\
                item delete <name> <id>\n\
                    sends request do delete message with index of <id> from board <name> on server\n\
                item update <name> <id> <content>\n\
                    sends request to change content of message with index <id> from board <name> to a <content>\n";
}

//search for argument to print help
int arghelp(int argc, char *argv[])
{
    for (int i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help")) {
            help();
            return 1;
        }
    }
    return 0;
}

//receiving of HTTP message
//first get full header of HTTP msg, then try
//to get body if content-length is defined.
string receive(int socket)
{
    char buffer[BUFFER+1] = {0};

    if (read(socket, buffer, BUFFER) == -1)
        return "Error: Missing message.\n";

    string msg = buffer;

    size_t head_end; //index of end of header of HTTP message
    while ((head_end = msg.find("\r\n\r\n")) == string::npos) {
        memset(buffer, 0, BUFFER);
        if (read(socket, buffer, BUFFER) == -1)
            return "Error: Head of msg not completed:\n" + msg;
        msg += buffer;
    }
    //make case insensitive part of HTTP header lowercase
    for (size_t i = msg.find("\r\n") + 2; i < head_end; ++i)
        ((char*)msg.c_str())[i] = tolower(msg.c_str()[i]);

    if (char *pom = strstr((char*)msg.c_str(), "content-length:")) {
        pom += 15;//after string "content-length:"
        //search for first digit, since there can be more of spaces
        while (*pom && !isdigit(*pom))
            ++pom;
        //count how big data should be read (with maximum of MAX_MSG_LENGTH)
        int loaded, ToRead = atoi(pom) - (msg.length() - head_end - 4);
        //if there is something to read, read it
        while (ToRead > 0) {
            memset(buffer, 0, BUFFER);
            if ((loaded = read(socket, buffer, (ToRead < BUFFER) ? ToRead : BUFFER)) == -1)
                return "Warning: Probably missing (part of) body.\n" + msg;
            
            ToRead -= loaded;
            msg += buffer;
        }
    }

    return msg;
}

//print HTTP header to stderr and HTTP body to stdout
int process(string msg)
{
    smatch match;
    if (regex_search(msg, match, regex("^.*\r\n\r\n", regex::extended))) {
        cerr << match.str(0);//header
        cout << &strstr(msg.c_str(), "\r\n\r\n")[4];//after header
        string header = regex_replace(match.str(0), regex("[ \t]+"), " ");//remove duplicate spaes
        if (regex_search(header, match, regex("^HTTP1/1 \\d\\d\\d[\r ]"))) {//check return code format
            if (match.str(0).c_str()[8] == '2')//check if return code is 2XX
                return 0;
        }
    } else
        cerr << msg << '\n';
    
    return -1;
}

//return predefined message from arguments, potencionally dangerous, need to control args before calling of this function
string msg_create(char *host, char *args[])
{
    if (!strcmp(args[0], "boards"))
        return (string)"GET /boards HTTP/1.1\r\nHost: " + host + "\r\n\r\n";
    else if (!strcmp(args[0], "board")) {
        if (!strcmp(args[1], "add"))
            return (string)"POST /boards/" + args[2] + " HTTP/1.1\r\nHost: " + host + "\r\n\r\n";
        else if (!strcmp(args[1], "delete"))
            return (string)"DELETE /boards/" + args[2] + " HTTP/1.1\r\nHost: " + host + "\r\n\r\n";
        else if (!strcmp(args[1], "list"))
            return (string)"GET /board/" + args[2] + " HTTP/1.1\r\nHost: " + host + "\r\n\r\n";
    } else if (!strcmp(args[0], "item")) {
        if (!strcmp(args[1], "add"))
            return (string)"POST /board/" + args[2] + " HTTP/1.1\r\nHost: " + host +
                "\r\nContent-Type: text/plain\r\nContent-Length: " + to_string(strlen(args[3])) + "\r\n\r\n" + args[3];
        else if (!strcmp(args[1], "delete"))
            return (string)"DELETE /board/" + args[2] + "/" + args[3] + " HTTP/1.1\r\nHost: " + host + "\r\n\r\n";
        else if (!strcmp(args[1], "update"))
            return (string)"PUT /board/" + args[2] + "/" + args[3] + " HTTP/1.1\r\nHost: " + host +
                "\r\nContent-Type: text/plain\r\nContent-Length: " + to_string(strlen(args[4])) + "\r\n\r\n" + args[4];
    }
    return "";
}

//create socket, connect to a server, send message and print response
int client(char *host, int port, string msg)
{
    int sock;
    struct sockaddr_in serv_addr = {AF_INET, htons(port), 0, 0};
    
    if (!host)
        handle_error("Host wasn't be translated to IP\n");

    //create socket
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) == -1)
        perr("socket");

    // Convert IPv4 addresses from text to binary form
    if (inet_pton(AF_INET, host, &serv_addr.sin_addr) != 1)
        perr("inet_pton");

    //connect socket
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) == -1)
        perr("connect");

    //send message
    if (write(sock, msg.c_str(), msg.length() + 1) != (int)msg.length() + 1)
        fprintf(stderr, "Warning: Partial/failed write\n");
    
    return process(receive(sock));
}

//do all argument controls
int main(int argc, char *argv[])
{
    if (arghelp(argc, argv))
        return 0;
    
    if (argc < 6)
        handle_error("Error: Missing arguments.\n");

    //with this can host and port be swapped when starting program
    if (!strcmp(argv[1], "-p")) {
        swap(argv[1], argv[3]);//swap -p and -H
        swap(argv[2], argv[4]);//swap <port> and <host>
    }

    if (strcmp(argv[1], "-H") || strcmp(argv[3], "-p"))
        handle_error("Error: Bad arguments.\n");
    
    //getting number of needed arguments based on command - boards -> 6, board -> 8, item -> 9, else fail
    size_t ctrl = !strcmp(argv[5], "boards") ? 6 : (!strcmp(argv[5], "board") ? 8 : (!strcmp(argv[5], "item") ? 9 : 0));
    //if command is item update, then one more argument is needed then for other item commands
    if (ctrl == 9 && !strcmp(argv[6], "update"))
        ctrl = 10;
 
    if ((int)ctrl != argc)
        handle_error("Error: Unsupported command \"" << argv[5] << "\" or wrong arguments for this command.\n");

    if (strcmp(argv[5], "boards") && strcmp(argv[6], "add") && strcmp(argv[6], "delete") && strcmp(argv[6], "list") && strcmp(argv[6], "update"))
        handle_error("Error: Unsupported command for " << argv[5] << ": \"" << argv[6] << "\".\n");

    //turns port from string to a number
    int port = stoi(argv[4], &ctrl);

    //control if everything was done right
    if (argv[4][ctrl] != '\0' || ctrl == 0)
        handle_error("Error: Port " << argv[4] << " is NaN.\n");
    
    if (port < 0 || port > 65535)
        handle_error("Error: Port is not in interval 0-65535.\n");

    return client(HostToIp(argv[2]), port, msg_create(argv[2], &argv[5]));
}
