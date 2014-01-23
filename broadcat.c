/*
 * broadcat.c -- a broadcast socket server

 * with help from Beej's Guide to Network Programming
 * http://beej.us/guide/bgnet/output/html/multipage/advanced.html#select
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <signal.h>

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int main(int argc, char *argv[])
{
    fd_set master;    // master file descriptor list
    fd_set read_fds;  // temp file descriptor list for select()
    int fdmax;        // maximum file descriptor number
    char *port;       // port we're listening on

    int num_clients;  // number of clients connected
    char **command;   // command and args, passed as argument
    int child_pid;    // pid of child process running command
    int c_pipe[2];    // pipe for communicating with child

    int listener;     // listening socket descriptor
    int newfd;        // newly accept()ed socket descriptor
    struct sockaddr_storage remoteaddr; // client address
    socklen_t addrlen;

    char buf[1];    // buffer for client data, which we don't actually read
    char cur_data[256];// buffer for server data
    int cur_data_len;

    char remoteIP[INET6_ADDRSTRLEN];

    int yes=1;        // for setsockopt() SO_REUSEADDR, below
    int i, j, rv;

    struct addrinfo hints, *ai, *p;

    if (argc <= 1) {
        fprintf(stderr, "Usage: %s port [command...]\n", argv[0]);
        return 1;
    }
    port = argv[1];

    if (argc > 2) {
        // got a child process to create
        command = &argv[2];

        // create communication pipe
		 if(pipe(c_pipe)){
			fprintf(stderr,"Pipe error!\n");
			exit(1);
		}

        // Start the child process
        if ((child_pid = fork()) == -1) {
            perror("fork");
            exit(1);        
        }
        if (!child_pid) {
			// child

			// Replace stdout with write end of pipe
			dup2(c_pipe[1],1);
			// Close read end of pipe
			close(c_pipe[0]);

            // run the command
            return execvp(command[0], command);
        }

		// Replace stdin with read end of pipe
		dup2(c_pipe[0],0);
		// Close write end of pipe
		close(c_pipe[1]);
    } else {
		command = NULL;
	}

    FD_ZERO(&master);    // clear the master and temp sets
    FD_ZERO(&read_fds);

    // start with empty line
    cur_data_len = 0;

    // get us a socket and bind it
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET6;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    if ((rv = getaddrinfo(NULL, port, &hints, &ai)) != 0) {
        fprintf(stderr, "%s: %s\n", argv[0], gai_strerror(rv));
        exit(1);
    }
    
    for(p = ai; p != NULL; p = p->ai_next) {
        listener = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (listener < 0) { 
            continue;
        }
        
        // lose the pesky "address already in use" error message
        setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));

        if (bind(listener, p->ai_addr, p->ai_addrlen) < 0) {
            close(listener);
            continue;
        }

        break;
    }

    // if we got here, it means we didn't get bound
    if (p == NULL) {
        fprintf(stderr, "%s: failed to bind\n", argv[0]);
        exit(2);
    }

    freeaddrinfo(ai); // all done with this

    // listen
    if (listen(listener, 10) == -1) {
        perror("listen");
        exit(3);
    }

    // add the listener to the master set
    FD_SET(listener, &master);

    // listen to stdin
    FD_SET(0, &master);

    // keep track of the biggest file descriptor
    fdmax = listener; // so far, it's this one

    // keep track of how many clients are connected
    num_clients = 0;

    // stop the process until we have a client connected
    if (command) {
        kill(child_pid, SIGSTOP);
    }

    // main loop
    for(;;) {
        read_fds = master; // copy it
        if (select(fdmax+1, &read_fds, NULL, NULL, NULL) == -1) {
            perror("select");
            exit(4);
        }

        // run through the existing connections looking for data to read
        for(i = 0; i <= fdmax; i++) {
            if (FD_ISSET(i, &read_fds)) { // we got one!!
                if (i == 0) {
                    // handle message from stdin
                    if (fgets(cur_data, sizeof cur_data, stdin) == NULL) {
                        if (ferror(stdin)) {
                            perror("fgets");
                            exit(1);
                        }
                        if (feof(stdin)) {
                            // need to clean up sockets?
                            exit(0);
                        }
                    } else {
                        // we got some data from stdin
                        cur_data_len = strlen(cur_data);
                        
                        for(j = 1; j <= fdmax; j++) {
                            // send to everyone!
                            if (FD_ISSET(j, &master)) {
                                // except the listener and ourselves
                                if (j != listener) {
                                    if (send(j, cur_data, cur_data_len, 0) == -1) {
                                        perror("send");
                                    }
                                }
                            }
                        }
                    }
                } else if (i == listener) {
                    // handle new connections
                    addrlen = sizeof remoteaddr;
                    newfd = accept(listener,
                        (struct sockaddr *)&remoteaddr,
                        &addrlen);

                    if (newfd == -1) {
                        perror("accept");
                    } else {
                        FD_SET(newfd, &master); // add to master set
                        if (newfd > fdmax) {    // keep track of the max
                            fdmax = newfd;
                        }
                        // send initial data
                        if (send(newfd, cur_data, cur_data_len, 0) == -1) {
                            perror("send");
                        }
                        //printf("client connected\n");
                        if (command && num_clients == 0) {
                            kill(child_pid, SIGCONT);
                        }
						num_clients++;
                        printf("client %d connected: %s\n", num_clients,
                            inet_ntop(remoteaddr.ss_family,
                                get_in_addr((struct sockaddr*)&remoteaddr),
                                remoteIP, INET6_ADDRSTRLEN));
                        /*
                        printf("%s: new connection from %s on "
                            "socket %d\n",
                            inet_ntop(remoteaddr.ss_family,
                                get_in_addr((struct sockaddr*)&remoteaddr),
                                remoteIP, INET6_ADDRSTRLEN),
                            newfd, argv[0]);
                            */
                    }
                } else {
                    // handle data from a client
                    if (recv(i, buf, sizeof buf, 0) <= 0) {
                        //printf("client disconnected\n");
                        // got error or connection closed by client
                        close(i); // bye!
                        FD_CLR(i, &master); // remove from master set

						num_clients--;
                        if (command && num_clients == 0) {
                            // stop the process
                            kill(child_pid, SIGSTOP);
                        }
                        printf("client %d disconnected\n", num_clients+1);
                    }
                } // END handle data from client
            } // END got new incoming connection
        } // END looping through file descriptors
    } // END for(;;)--and you thought it would never end!
    
    return 0;
}
