/*
 *  File:       isaserver.cpp
 *  Solution:   ISA - HTTP nastenka
 *  Author:     Radek Duchon - xducho07, VUT FIT 3BIT 2019/20
 *  Compiled:   g++ 7.4.0
 *  Date:       17.11.2019
 */

#include <iostream>
#include <sys/un.h>
#include <netinet/in.h>
#include <unistd.h>
#include <regex>
#include <map>
#include <vector>
#include "isaserver.hpp"

using namespace std;

#define LISTEN_BACKLOG 10
#define BUFFER 16384
#define MAX_MSG_LENGTH 32768

//print error message and help, then return 1
#define handle_error(msg) \
    do {\
        cerr << msg;\
        help();\
        return 1;\
    } while (0)

//call perror for predefined errors and print help, then return
//'v' stands for void
#define vperr(msg) \
    do {\
        perror(msg);\
        help();\
        return;\
    } while (0)

//determines if messages should be written (under argument -v)
static bool verbose = false;

//print help
void help(void)
{
    cout << "starting the program:\n\t./isaserver -p <port> [-v | --verbose]\n\
        <port> is number from 0 to 65535, alltrough ports 0-1023 are restricted just for root.\n\
        With argument --verbose or -v, the server will print all incoming and outcoming messages to stdout.\n";
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

    if (read(socket, buffer, BUFFER) == -1) {
        cerr << "Error: Missing message.\n";
        return "";
    }

    string msg = buffer;

    size_t head_end; //index of end of header of HTTP message
    while ((head_end = msg.find("\r\n\r\n")) == string::npos) {
        memset(buffer, 0, BUFFER);
        if (read(socket, buffer, BUFFER) == -1) {
            cerr << "Error: Head of msg not completed\nMSG:\n" << msg << "\nMSG END\n";
            return "";
        }
        msg += buffer;
        if (msg.length() > MAX_MSG_LENGTH) {
            cerr << "Error: message is suspicous, it won't be processed\nMSG:\n" << msg << "\nMSG END\n";
            return "";
        }
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
        int loaded, ToRead = (atoi(pom) > MAX_MSG_LENGTH ? MAX_MSG_LENGTH : atoi(pom)) - (msg.length() - head_end - 4);
        //if there is something to read, read it
        while (ToRead > 0) {
            memset(buffer, 0, BUFFER);
            if ((loaded = read(socket, buffer, (ToRead < BUFFER) ? ToRead : BUFFER)) == -1) {
                cerr << "Error: Probably missing (part of) body.\nMSG:\n" << msg << "\nMSG END\n";
                return "";
            }
            
            ToRead -= loaded;
            msg += buffer;
        }
    }

    return msg;
}

//return response to client based on request and actual state of boards
string response(string msg)
{
    static map<string, vector<string>> boards;
    string answer, header;
    smatch match;
    const char *m;
 
    //make copy of HTTP header
    regex_search(msg, match, regex("^.*\r\n\r\n", regex::extended));
    //eliminate duplicate spaces, vertical tabulators are replaced as spaces
    header = regex_replace(match.str(0), regex("[ \t]+"), " ");
    
    //boards
    if (regex_search(header, regex("^GET /boards HTTP/1.1 ?\r\n"))) {
        for (auto board : boards)
            answer += board.first + "\n";
        
        return "HTTP1/1 200 OK\r\nContent-Length: " + to_string(answer.length()) + "\r\n\r\n" + answer;
    }

    //board add <name>
    if (regex_search(header, regex("^POST /boards/\\w* HTTP/1.1 ?\r\n"))) {
        regex_search(header, match, regex("/boards/\\w*"));//getting "/boards/<name>"
        m = &match.str(0).c_str()[8];//after "/boards/"
        if (boards.count(m))//check if board m exists ("\\w*" part of regex)
            return "HTTP1/1 409 Conflict\r\n\r\n";
        //creates board m ("\\w*" part of regex)
        boards[m];
        return "HTTP1/1 201 Created\r\n\r\n";
    }

    //board detele <name>
    if (regex_search(header, regex("^DELETE /boards/\\w* HTTP/1.1 ?\r\n"))) {
        regex_search(header, match, regex("/boards/\\w*"));//getting "/boards/<name>"
        m = &match.str(0).c_str()[8];//after "/boards/"
        if (boards.count(m)) {//check if board m exists ("\\w*" part of regex)
            boards.erase(m);
            return "HTTP1/1 200 OK\r\n\r\n";
        }
        return "HTTP1/1 404 Not Found\r\n\r\n";
    }

    //board list <name>
    if (regex_search(header, regex("^GET /board/\\w* HTTP/1.1 ?\r\n"))) {
        regex_search(header, match, regex("/board/\\w*"));//getting "/board/<name>"
        m = &match.str(0).c_str()[7];//after "/board/"
        if (boards.count(m)) {//check if board m exists ("\\w*" part of regex)
            answer = string("[") + m + "]\n";
            unsigned i = 0;
            for (auto &board : boards[m])
                answer += to_string(++i) + ". " + board + "\n";
            
            return "HTTP1/1 200 OK\r\nContent-Length: " + to_string(answer.length()) + "\r\n\r\n" + answer;
        }
        return "HTTP1/1 404 Not Found\r\n\r\n";
    }

    //item add <name> <content>
    if (regex_search(header, regex("^POST /board/\\w* HTTP/1.1 ?\r\n"))) {
        regex_search(header, match, regex("content-length: ?\\d+"));
        m = &match.str(0).c_str()[15];//after "content-length:"
        if (!atoi(m) && !atoi(m+1))//not sure, if after content-length is space or not, so try both
            return "HTTP1/1 400 Bad Request\r\n\r\n";

        regex_search(header, match, regex("/board/\\w*"));//getting "/board/<name>"
        m = &match.str(0).c_str()[7];
        if (boards.count(m)) {//check if board m exists ("\\w*" part of regex)
            boards[m].push_back(&strstr(msg.c_str(), "\r\n\r\n")[4]);
            return "HTTP1/1 201 Created\r\n\r\n";
        }
        return "HTTP1/1 404 Not Found\r\n\r\n";
    }

    //item update <name> <id> <content>
    if (regex_search(header, regex("^PUT /board/\\w*/\\d+ HTTP/1.1 ?\r\n"))) {
        regex_search(header, match, regex("content-length: ?\\d+"));
        m = &match.str(0).c_str()[15];//after "content-length:"
        if (!atoi(m) && !atoi(m+1))//not sure, if after content-length is space or not, so try both
            return "HTTP1/1 400 Bad Request\r\n\r\n";

        regex_search(header, match, regex("/board/\\w*"));//getting "/board/<name>"
        m = &match.str(0).c_str()[7];//after "/board/"
        //atoi(after '/' after '/' after '/') == atoi after "/board/<name>/" where is id
        unsigned idx = atoi(&strstr(strstr(strstr(header.c_str(), "/")+1, "/")+1, "/")[1]) - 1;
        if (boards.count(m) && idx < boards[m].size()) {//check if board m exists ("\\w*" part of regex)
            boards[m][idx] = (&strstr(msg.c_str(), "\r\n\r\n")[4]);
            return "HTTP1/1 200 OK\r\n\r\n";
        }
        return "HTTP1/1 404 Not Found\r\n\r\n";
    }

    //item delete <name> <id>
    if (regex_search(header, regex("^DELETE /board/\\w*/\\d+ HTTP/1.1 ?\r\n"))) {
        regex_search(header, match, regex("/board/\\w*"));//getting "/board/<name>"
        m = &match.str(0).c_str()[7];//after "/board/"
        //atoi(after '/' after '/' after '/') == atoi after "/board/<name>/" where is id
        unsigned idx = atoi(&strstr(strstr(strstr(header.c_str(), "/")+1, "/")+1, "/")[1]) - 1;
        if (boards.count(m) && idx < boards[m].size()) {//check if board m exists ("\\w*" part of regex)
            boards[m].erase(boards[m].begin() + idx);
            return "HTTP1/1 200 OK\r\n\r\n";
        }
        return "HTTP1/1 404 Not Found\r\n\r\n";
    }

    return "HTTP1/1 404 Not Found\r\n\r\n";
}

//run server at specified port
void server(int port)
{
    int sfd, new_socket;
    struct sockaddr_in address = {AF_INET, htons(port), htonl(INADDR_ANY), 0};
    struct timeval timeout = {0, 500};//seconds, microseconds
    int addrlen = sizeof(address);
 
    //create socket
    if ((sfd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
        vperr("socket");

    //assign address to socket
    if (bind(sfd,(struct sockaddr *)&address, sizeof(struct sockaddr_un)) ==  -1)
        vperr("bind");

    //marks socket as pssive - used for accepting incoming connections and set maximum length of queue of incomming connections to LISTEN_BACKLOG
    if (listen(sfd, LISTEN_BACKLOG) == -1)
        vperr("listen");

    while (1) {
        //extracts first connection from queue and creates connected socket
        if ((new_socket = accept(sfd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) == -1)
            vperr("accept"); 
        //setting socket to wait for incomming data just for a limited time defined by timeout
        if (setsockopt(new_socket, SOL_SOCKET, SO_RCVTIMEO, (const void*)&timeout, sizeof(struct timeval)) == -1)
            vperr("setsockopt"); 

        string msg = receive(new_socket);
        string answer = response(msg);
        //print messages if server is launched with argument -v
        if (verbose)
            cout << "MESSAGE:\n" << msg << "\nRESPONSE:\n" << answer;

        write(new_socket, answer.c_str(), answer.length());
        close(new_socket);
    }
}

//do argument controls, then calls server with needed informations to run it
int main(int argc, char *argv[])
{
    if (arghelp(argc, argv))
        return 0;

    //adding compatibilityy for -v as first argument by changing order of arguments
    if (argc == 4 && !strcmp(argv[1], "-v")) {
        swap(argv[1], argv[3]);//-v to argv[3]
        swap(argv[1], argv[2]);//<port> -p to -p <port>
    }

    if (argc < 3 || argc > 4 || strcmp(argv[1], "-p"))
        handle_error("Error: Bad arguments.\n");

    if (argc == 4) {
        if(!strcmp(argv[3], "-v") || !strcmp(argv[3], "--verbose"))
            verbose = true;
        else
            handle_error("Error: Bad arguments.\n");
    }

    size_t sz;
    int port = stoi(argv[2], &sz);

    if (argv[2][sz] != '\0' || sz == 0)
        handle_error("Error: Port " << argv[2] << " is NaN.\n");

    if (port < 0 || port > 65535)
        handle_error("Error: Port is not in interval 0-65535.\n");

    server(port);

    return 1;//it should not happen
}
