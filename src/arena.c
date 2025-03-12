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

#define SERVER_PORT "8080"

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

const char *determine_winner(player_info* p1, player_info* p2) {
  const char *choice1 = p1->choice;
  const char *choice2 = p2->choice;

  if (strcmp(choice1, choice2) == 0) {
    return "Nobody";
  }

  if ((strcmp(choice1, "ROCK") == 0 && strcmp(choice2, "SCISSORS") == 0) ||
      (strcmp(choice1, "PAPER") == 0 && strcmp(choice2, "ROCK") == 0) ||
      (strcmp(choice1, "SCISSORS") == 0 && strcmp(choice2, "PAPER") == 0)) {
    return p1->name;
  } else {
    return p2->name;
  }
}

/************************************************************************
 * Notification manager, which waits for jobs to be passed onto it and
 * then executes them.
 */
void *notif_manager(void *none) {
  while (1) {
    job *job = queue_dequeue_wait();

    switch (job->type) {
      case JOB_DONE:
        destroyjob(job);
        pthread_exit(NULL);
        break;

      case JOB_MSG: {
        player_info *origin = job->origin;
        player_info *target = playerlist_findplayer(job->to.player_name);
        if (target != NULL && target->in_room == origin->in_room &&
            target != origin) {
          send_notice(target, "From %s: %s", job->origin, job->content);
        }
        break;
      }

      case JOB_JOIN: {
        int newroom = job->to.room;
        player_info *mover = job->origin;
        int size = playerlist_getsize();
        for (size_t i = 0; i < size; i++) {
          player_info *curr = playerlist_get(i);
          if (curr->in_room == newroom && curr->state == PLAYER_REG) {
            if (newroom == 0) {
              send_notice(curr, "%s has joined the lobby", mover->name);
            } else {
              send_notice(curr, "%s has joined arena %d", mover->name, newroom);
            }
          }
        }
        break;
      }

      case JOB_LEAVE: {
        int oldroom = job->to.room;
        player_info *mover = job->origin;
        int size = playerlist_getsize();
        for (size_t i = 0; i < size; i++) {
          player_info *curr = playerlist_get(i);
          if (curr->in_room == oldroom) {
            if (oldroom == 0) {
              send_notice(curr, "%s has left the lobby", mover->name);
            } else {
              send_notice(curr, "%s has left arena %d", mover->name, oldroom);
            }
          }
        }
        break;
      }

      case JOB_CHALLENGE: {
        player_info *challenger = job->origin;
        player_info *target = playerlist_findplayer(job->to.player_name);
        if (target != NULL && challenger != target &&
            challenger->in_room == target->in_room) {
          send_notice(
              target,
              "%s has challenged you to a duel. Please ACCEPT or REJECT",
              challenger->name);
          
          target->duel_status = DUEL_PENDING;
          target->opponent = challenger;

          challenger->duel_status = DUEL_PENDING;
          challenger->opponent = target;
          
        }
        break;
      }

      case JOB_ACCEPT: {
        player_info *accepter = job->origin;
        player_info *challenger = accepter->opponent;
        if (challenger != NULL && accepter != challenger &&
            accepter->in_room == challenger->in_room) {
          send_notice(challenger,
                      "%s has accepted your challenge. Let the battle begin!",
                      accepter->name);

          accepter->duel_status = DUEL_ACTIVE;
          send_notice(accepter, "Please CHOOSE ROCK, PAPER, or SCISSORS.");
          
          challenger->duel_status = DUEL_ACTIVE;
          send_notice(challenger, "Please CHOOSE ROCK, PAPER, or SCISSORS.");
        }
        break;
      }

      case JOB_REJECT: {
        player_info *rejecter = job->origin;
        player_info *challenger = rejecter->opponent;
        if (challenger != NULL && rejecter != challenger &&
            rejecter->in_room == challenger->in_room) {
          send_notice(challenger, "%s has rejected your challenge.",
                      rejecter->name);
          rejecter->duel_status = DUEL_NONE;
          challenger->duel_status= DUEL_NONE;
        }
        break;
      }

      case JOB_CHOICE: {
        player_info *p1 = job->origin;
        player_info *p2 = p1->opponent;
        if (p1->choice != NULL && p2->choice != NULL) { // may cause race condition? never seen it happen but slightly sketch...
          const char *winner = determine_winner(p1, p2);
          send_notice(p1, "Result of your duel with %s: %s wins!", p2->name, winner);
          send_notice(p2, "Result of your duel with %s: %s wins!", p1->name, winner);

          p1->choice = NULL;
          p1->duel_status = DUEL_NONE;

          p2->choice = NULL;
          p2->duel_status = DUEL_NONE;
        }
        break;
      }

      default: // unreachable unless i made a typo!
        break;
    }

    destroyjob(job);
  }

  pthread_exit(NULL);
}

/************************************************************************
 * Signal handler for SIGINT to allow server to exit more gracefully.
 * TODO: is there a good way to also kill all active player threads using this
 * variable? Because they are currently blocking while waiting for input (for
 * player to enter a command/disconnect).
 */
volatile sig_atomic_t done = 0;
void terminate_server(int sig) {
  done = 1;
  job *job = newjob(JOB_DONE, NULL, NULL, NULL);
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
  int sock_fd = create_listener(SERVER_PORT);
  if (sock_fd < 0) {
    fprintf(stderr, "Server setup failed.\n");
    exit(1);
  }

  /* Set up signal handler to handle SIGINT so resources can be freed when
   * program exits */
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
  while ((comm_fd = accept(sock_fd, (struct sockaddr *)&client_addr,
                           &client_addr_len)) >= 0 &&
         !done) {
    printf("Got connection from %s\n",
           inet_ntoa(((struct sockaddr_in *)&client_addr)->sin_addr));

    player_info *newplayer = new_player(comm_fd);

    pthread_t new_thread;
    pret = 0;
    if ((pret = pthread_create(&new_thread, NULL, &handle_player, newplayer)) <
        0) {
      perror("pthread_create");
      exit(1);
    }
  }

  pthread_join(notif, NULL);

  queue_destroy();
  playerlist_destroy();

  return 0;
}
