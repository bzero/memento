#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <fcntl.h>
#include <time.h>
#include <sys/time.h>

#define CMD_BUFSIZE 1024
#define VERSION "0.0.1"

void banner(void);
void help(void);
void command_loop(int, char *, char *);
char *read_command(void);

int main(int argc, char **argv) {
    int sock_fd, port;
    struct sockaddr_in serveraddr;
    struct hostent *server;
    char *hostname;

    /* check command line arguments */
    if (argc != 3) {
        fprintf(stderr,"usage: %s <hostname> <port>\n", argv[0]);
        exit(0);
    }
    hostname = argv[1];
    port = atoi(argv[2]);

    /* socket: create the socket */
    sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_fd < 0)
        perror("ERROR opening socket");

    /* gethostbyname: get the server's DNS entry */
    server = gethostbyname(hostname);
    if (server == NULL) {
        fprintf(stderr,"ERROR, no such host as %s\n", hostname);
        exit(0);
    }

    /* build the server's address */
    bzero((char *) &serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    bcopy((char *)server->h_addr,
          (char *)&serveraddr.sin_addr.s_addr, server->h_length);
    serveraddr.sin_port = htons(port);

    /* connect: create a connection with the server */
    if (connect(sock_fd, (const struct sockaddr *) &serveraddr, sizeof(serveraddr)) < 0) {
        perror("ERROR connecting");
        exit(0);
    }

    /* fcntl(sock_fd, F_SETFL, O_NONBLOCK); */

    char c_port[4];
    sprintf(c_port, "%d", port);
    banner();
    command_loop(sock_fd, hostname, c_port);
    return EXIT_SUCCESS;
}

// loop for accepting, parsing and sending commands to the memento server
// instance, reading response
void command_loop(int fd, char *hostname, char *port) {
    char *line, buf[CMD_BUFSIZE];
    int status = 1;
    // while condition: status > 0
    while (status) {
        printf("%s:%s> ", hostname, port);
        line = read_command();
        char supp_command[strlen(line)];
        bzero(supp_command, strlen(line));
        strcpy(supp_command, line);
        char *comm = strtok(supp_command, " ");
        if (strlen(line) > 1) {
            char token[4];
            strncpy(token, line, 4);
            if (strncasecmp(token, "SUB ", 4) == 0 || strncasecmp(token, "TAIL", 4) == 0) {
                // write to server
                status = write(fd, line, strlen(line));
                if (status < 0)
                    perror("ERROR writing to socket");
                while (1) {
                    /* print the server's reply */
                    bzero(buf, CMD_BUFSIZE);
                    status = read(fd, buf, CMD_BUFSIZE);
                    if (strncasecmp(buf, "NOT FOUND", 9) == 0) break;
                    printf("%s", buf);
                }
            } else if (strncasecmp(comm, "KEYS", 4) == 0 ||
                       strncasecmp(comm, "VALUES", 6) == 0) {
                fcntl(fd, F_SETFL, O_NONBLOCK);
                status = write(fd, line, strlen(line));
                usleep(100000);
                if (status < 0)
                    perror("ERROR writing to socket");
                while(read(fd, buf, CMD_BUFSIZE) > 0) {
                    printf("%s", buf);
                    bzero(buf, CMD_BUFSIZE);
                }
            } else if (token[0] == '?' ||
                       token[0] == 'h' ||
                       token[0] == 'H' ||
                       strncasecmp(token, "HELP", 4) == 0) {
                help();
                bzero(buf, CMD_BUFSIZE);
            /* } else if (strncasecmp(comm, "BENCHMARK", 9) == 0) { */
            /*     /\* clock_t start_t, end_t; *\/ */
            /*     /\* start_t = clock(); *\/ */
            /*     struct timeval start, stop; */
            /*     double secs = 0; */

            /*     gettimeofday(&start, NULL); */

            /*     for (int i = 0; i < 30000; i++) { */
            /*         /\* char payload[30]; *\/ */
            /*         /\* sprintf(payload, "set key%d value%d\r\n", i, i); *\/ */
            /*         /\* printf("%s\n", payload); *\/ */
            /*         char *payload = "set key value\r\n"; */
            /*         status = write(fd, payload, 15); */
            /*         /\* bzero(buf, CMD_BUFSIZE); *\/ */
            /*         /\* status = read(fd, buf, CMD_BUFSIZE); *\/ */
            /*     } */
            /*     /\* end_t = clock(); *\/ */
            /*     /\* double delta_t = (double) (end_t - start_t) / CLOCKS_PER_SEC; *\/ */
            /*     gettimeofday(&stop, NULL); */
            /*     secs = (double)(stop.tv_usec - start.tv_usec) / 1000000 + (double)(stop.tv_sec - start.tv_sec); */
            /*     printf("Nr. of set ops: %d\n", 30000); */
            /*     printf("CPU time: %lf\n", secs); */
            } else {
                // write to server
                status = write(fd, line, strlen(line));
                if (status < 0)
                    perror("ERROR writing to socket");
                /* print the server's reply */
                bzero(buf, CMD_BUFSIZE);
                status = read(fd, buf, CMD_BUFSIZE);
                if (status < 0)
                    perror("ERROR reading from socket");
                printf("%s", buf);
            }
            free(line);
        }
    }
}
// read a single string containing command
char *read_command(void) {
    char *line = NULL;
    size_t bufsize = 0;
    getline(&line, &bufsize, stdin);
    return line;
}
// print initial banner
void banner(void) {
    printf("\n");
    printf("Memento CLI v %s\n", VERSION);
    printf("Type h or ? or help to print command list\n\n");
}
// print help
void help(void) {
    printf("\n");
    printf("SET key value               Sets <key> to <value>\n");
    printf("GET key                     Get the value identified by <key>\n");
    printf("DEL key key2 .. keyN        Delete values identified by <key>..<keyN>\n");
    printf("INC key qty                 Increment by <qty> the value idenfied by <key>, if\n");
    printf("                            no <qty> is specified increment by 1\n");
    printf("DEC key qty                 Decrement by <qty> the value idenfied by <key>, if\n");
    printf("                            no <qty> is specified decrement by 1\n");
    printf("INCF                        key qty Increment by float <qty> the value identified\n");
    printf("                            by <key>, if no <qty> is specified increment by 1.0\n");
    printf("DECF                        key qty Decrement by <qty> the value identified by <key>,\n");
    printf("                            if no <qty> is specified decrement by 1.0\n");
    printf("GETP key                    Get all information of a key-value pair represented by\n");
    printf("                            <key>, like key, value, creation time and expire time\n");
    printf("APPEND key value            Append <value> to <key>\n");
    printf("PREPEND key value           Prepend <value> to <key>\n");
    printf("KEYS                        List all keys stored into the keyspace\n");
    printf("VALUES                      List all values stored into the keyspace\n");
    printf("COUNT                       Return the number of key-value pair stored\n");
    printf("FLUSH                       Delete all maps stored inside partitions\n");
    printf("QUIT/EXIT                   Close connection\n");
    printf("\n");
}
