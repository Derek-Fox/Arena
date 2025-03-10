/* This is the main program for the arena server
 * The job of this module is to set the system up and then turn each
 * command received from the client over to the arena_protocol module
 * which will handle the actual communication protocol between clients
 * (players) and the server.
 */
#define _GNU_SOURCE

#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "arena_protocol.h"
#include "player.h"
#include "playerlist.h"
#include "queue.h"

#define SERV_PORT "8080"

/************************************************************************
 * Make a TCP listener for port "service" (given as a string, but
 * either a port number or service name). This function will only
 * create a public listener (listening on all interfaces).
 *
 * Either returns a file handle to use with accept(), or -1 on error.
 * In general, error reporting could be improved, but this just indicates
 * success or failure.
 */
int create_listener(char *service) {
    int sock_fd;
    if ((sock_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket");
        return -1;
    }

    // Avoid time delay in reusing port - important for debugging, but
    // probably not used in a production server.

    int optval = 1;
    setsockopt(sock_fd, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval));

    // First, use getaddrinfo() to fill in address struct for later bind

    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_flags = AI_PASSIVE;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = 0;

    struct addrinfo *result;
    int rval;
    if ((rval = getaddrinfo(NULL, service, &hints, &result)) != 0) {
        fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(rval));
        close(sock_fd);
        return -1;
    }

    // Assign a name/addr to the socket - just blindly grabs first result
    // off linked list, but really should be exactly one struct returned.

    int bret = bind(sock_fd, result->ai_addr, result->ai_addrlen);
    freeaddrinfo(result);
    result = NULL;  // Not really necessary, but ensures no use-after-free

    if (bret < 0) {
        perror("bind");
        close(sock_fd);
        return -1;
    }

    // Finally, set up listener connection queue
    int lret = listen(sock_fd, 128);
    if (lret < 0) {
        perror("listen");
        close(sock_fd);
        return -1;
    }

    return sock_fd;
}

/************************************************************************
 * Code that is run by each player thread. Reads input commands and sends
 * them to the notification manager. Also responsible for adding/removing
 * player to the global player list.
 */
void *handle_player(void *newplayer) {
    player_info *player = (player_info *)newplayer;
    playerlist_addplayer(player);

    char *lineptr = NULL;
    size_t linesize = 0;

    while (player->state != PLAYER_DONE) {
        if (getline(&lineptr, &linesize, player->fp_recv) < 0) {
            // Failed getline means the client disconnected
            break;
        }
        docommand(player, lineptr);
    }

    /* Finished with session, so unregister it and free resources. */
    if (lineptr != NULL) {
        free(lineptr);
    }

    playerlist_removeplayer(player);
    free(player);

    int pret = 0;
    if ((pret = pthread_detach(pthread_self())) != 0) {
        perror("pthread_detach");
        exit(1);
    }

    pthread_exit(NULL);
}

/************************************************************************
 * Notification manager, which waits for jobs to be passed onto it and
 * then executes them.
 */
void *notif_manager(void *none) {
    while (1) {
        job *job = queue_dequeue_wait();

        if (job->type == DONE) {
            destroyjob(job);
            break;
        }

        if (job->type == MSG) {
            player_info *target = playerlist_findplayer(job->to);
            player_info *origin = playerlist_findplayer(job->origin);
            if (target != NULL && target->in_room == origin->in_room && target != origin) {
                char *msg;
                asprintf(&msg, "From %s: %s", job->origin, job->content);
                send_notice(target, msg);
                free(msg);
            }
        } else if (job->type == JOIN) {
            int newroom = (int)strtol(job->to, NULL, 0);
            player_info *mover = playerlist_findplayer(job->origin);
            char *response;
            int size = playerlist_getsize();
            for (size_t i = 0; i < size; i++) {
                player_info *curr = playerlist_get(i);
                if (curr->in_room == newroom && curr->state == PLAYER_REG) {  // player in newroom -> notify join
                    if (newroom == 0) {                                       // joining lobby
                        asprintf(&response, "%s has joined the lobby", mover->name);
                    } else {
                        asprintf(&response, "%s has entered arena %d", mover->name, newroom);
                    }
                    send_notice(curr, response);
                    free(response);
                }
            }
        } else if (job->type == LEAVE) {
            int oldroom = (int)strtol(job->to, NULL, 0);
            player_info *mover = playerlist_findplayer(job->origin);
            char *response;
            int size = playerlist_getsize();
            for (size_t i = 0; i < size; i++) {
                player_info *curr = playerlist_get(i);
                if (curr->in_room == oldroom) {  // player in oldroom -> notify leave
                    if (oldroom == 0) {          // leaving lobby
                        asprintf(&response, "%s has left the lobby", mover->name);
                    } else {
                        asprintf(&response, "%s has left arena %d", mover->name, oldroom);
                    }
                    send_notice(curr, response);
                    free(response);
                }
            }
        } else if (job->type == CHALLENGE) {
            player_info *challenger = playerlist_findplayer(job->origin);
            player_info *target = playerlist_findplayer(job->to);
            if (target != NULL && challenger != target && challenger->in_room == target->in_room) {
                char *response;
                asprintf(&response, "%s has challenged you. Please ACCEPT or REJECT.", challenger->name);
                send_notice(target, response);
                free(response);
            }
        } else if (job->type == ACCEPT) {
            player_info *accepter = playerlist_findplayer(job->origin);
            player_info *challenger = playerlist_findplayer(accepter->challenge_from);
            if (challenger != NULL && accepter != challenger && accepter->in_room == challenger->in_room) {
                char *response;
                asprintf(&response, "%s has accepted your challenge. Let the battle begin!", accepter->name);
                send_notice(challenger, response);
                free(response);
            }
        } else if (job->type == REJECT) {
            player_info *rejecter = playerlist_findplayer(job->origin);
            player_info *challenger = playerlist_findplayer(rejecter->challenge_from);
            if (challenger != NULL && rejecter != challenger && rejecter->in_room == challenger->in_room) {
                char *response;
                asprintf(&response, "%s has rejected your challenge.", rejecter->name);
                send_notice(challenger, response);
                free(response);
            }
        } else if (job->type == RESULT) {
            player_info *winner = playerlist_findplayer(job->to);
            player_info *loser = playerlist_findplayer(job->content);
            char *response;
            asprintf(&response, "%s has defeated %s in the duel!", winner->name, loser->name);
            send_notice(winner, response);
            send_notice(loser, response);
            free(response);
        }
        destroyjob(job);
    }

    pthread_exit(NULL);
}

/************************************************************************
 * Signal handler for SIGINT to allow server to exit more gracefully.
 * TODO: is there a good way to also kill all active player threads using this variable?
 * Because they are currently blocking while waiting for input (for player to enter a command/disconnect).
 */
volatile sig_atomic_t done = 0;
void terminate_server(int sig) {
    done = 1;
    job *job = newjob(DONE, NULL, NULL, NULL);
    queue_enqueue(job);
    return;
}

/************************************************************************
 * Part 3 main: initializes playerlist, starts notification manager,
 * sets up signal handler, starts TCP server and waits for connections.
 * Then creates threads to handle each connection.
 */
int main(int argc, char *argv[]) {
    /* Set up global playerlist */
    playerlist_init();

    /* Set up server to start accepting connections */
    int sock_fd = create_listener(SERV_PORT);
    if (sock_fd < 0) {
        fprintf(stderr, "Server setup failed.\n");
        exit(1);
    }

    /* Set up signal handler to handle SIGINT so resources can be freed when program exits */
    struct sigaction sa;
    sa.sa_handler = terminate_server;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESETHAND;
    sigaction(SIGINT, &sa, NULL);

    /* Set up notification manager thread and job queue*/
    queue_init();
    pthread_t notif;
    int pret = 0;
    if ((pret = pthread_create(&notif, NULL, notif_manager, NULL)) < 0) {
        perror("pthread_create");
        exit(1);
    }

    struct sockaddr_storage client_addr;
    socklen_t client_addr_len = sizeof(client_addr);
    int comm_fd;
    while ((comm_fd = accept(sock_fd, (struct sockaddr *)&client_addr, &client_addr_len)) >= 0 && !done) {
        printf("Got connection from %s\n",
               inet_ntoa(((struct sockaddr_in *)&client_addr)->sin_addr));

        player_info *newplayer = new_player(comm_fd);

        pthread_t new_thread;
        pret = 0;
        if ((pret = pthread_create(&new_thread, NULL, &handle_player, newplayer)) < 0) {
            perror("pthread_create");
            exit(1);
        }
    }

    pthread_join(notif, NULL);

    queue_destroy();
    playerlist_destroy();

    return 0;
}
