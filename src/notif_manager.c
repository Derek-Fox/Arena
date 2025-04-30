#include "notif_manager.h"

#include <stdlib.h>
#include <string.h>

#include "arena_protocol.h"
#include "playerlist.h"

// Forward declarations of functions to handle each job type
static void handle_job_msg(job* job);
static void handle_job_join(job* job);
static void handle_job_leave(job* job);
static void handle_job_challenge(job* job);
static void handle_job_accept(job* job);
static void handle_job_reject(job* job);
static void handle_job_choice(job* job);
static void handle_job_broadcast(job* job);

// Array of function pointers from above
static void (*job_handlers[])(job*) = {
    handle_job_msg,       handle_job_join,      handle_job_leave,
    handle_job_challenge, handle_job_accept,    handle_job_reject,
    handle_job_choice,    handle_job_broadcast,
};

/************************************
 * Read a job off the queue when it arrives and call the appropriate handler
 * function.
 */
static void notif_loop() {
  while (1) {
    job* job = queue_dequeue_wait();

    if (job->type > 0 &&
        job->type <= sizeof(job_handlers) / sizeof(job_handlers[0])) {
      job_handlers[job->type - 1](job);  // -1 because job_done would be in slot
                                         // 0, but it needs no handler
    } else if (job->type == JOB_DONE) {
      destroyjob(job);
      return;
    }

    destroyjob(job);
  }
}

static void handle_job_msg(job* job) {
  player_info* from = job->origin;
  player_info* to = playerlist_findplayer(job->to.player_name);
  if (to == NULL) {
    send_err(from, "Cannot find player %s.", job->to.player_name);
  } else if (from == to) {
    send_err(from, "Cannot MSG yourself. Stop.");
  } else if (to->state != PLAYER_REG) {
    send_err(from, "%s is not logged in.", to->name);  // should be impossible?
  } else if (to->in_room != from->in_room) {
    send_err(from, "%s is not in your arena, cannot send message.", to->name);
  } else {
    send_notice(to, "From %s: %s", job->origin->name, job->content);
  }
}

static void join_leave_helper(int room, const char* mover_name,
                              const char* join_leave) {
  int size = playerlist_getsize();
  for (size_t i = 0; i < size; i++) {
    player_info* curr = playerlist_get(i);
    if (curr->in_room != room || curr->state != PLAYER_REG) continue;
    if (room == ROOM_LOBBY)
      send_notice(curr, "%s has %s the lobby.", mover_name, join_leave);
    else
      send_notice(curr, "%s has %s arena %d.", mover_name, join_leave, room);
  }
}

static void handle_job_join(job* job) {
  join_leave_helper(job->to.room, job->origin->name, "joined");
}

static void handle_job_leave(job* job) {
  join_leave_helper(job->to.room, job->origin->name, "left");
}

static void handle_job_challenge(job* job) {
  player_info* challenger = job->origin;
  player_info* target = playerlist_findplayer(job->to.player_name);
  if (target == NULL) {
    fprintf(stderr, "Malformed job on queue, type CHALLENGE. Origin: %s",
            challenger->name);
  } else if (target == challenger) {
    send_err(challenger, "Cannot challenge yourself. Stop.");
  } else if (target->state != PLAYER_REG) {
    send_err(challenger, "%s does not match the name of a logged in player.",
             target->name);
  } else if (target->in_room != challenger->in_room) {
    send_err(challenger, "%s is not in your arena, cannot send challenge.",
             target->name);
  } else {
    send_notice(target,
                "%s has challenged you to a duel. Please ACCEPT or REJECT",
                job->origin->name);
    target->duel_status = DUEL_PENDING;
    target->opponent = challenger;
    challenger->duel_status = DUEL_PENDING;
    challenger->opponent = target;
  }
}

// TODO: finish this and all other job functions
static void handle_job_accept(job* job) {
  player_info* accepter = job->origin;
  player_info* challenger = accepter->opponent;
  if (challenger == NULL) {
    fprintf(stderr, "Malformed job on queue, type ACCEPT. Origin: %s",
            accepter->name);
  } else if (challenger->in_room != accepter->in_room) {
    send_err(accepter,
             "%s has left your arena! Cannot accept their challenge. Move "
             "to their arena and try again");
  } else {
    send_notice(
        accepter,
        "You have accepted the challenge from %s. Let the battle begin!",
        challenger->name);
    send_notice(challenger,
                "%s has accepted your challenge. Let the battle begin!",
                accepter->name);

    accepter->duel_status = DUEL_ACTIVE;
    send_notice(accepter, "Please CHOOSE from ROCK, PAPER, or SCISSORS.");
    challenger->duel_status = DUEL_ACTIVE;
    send_notice(challenger, "Please CHOOSE from ROCK, PAPER, or SCISSORS.");
  }
}

static void handle_job_reject(job* job) {
  player_info* rejecter = job->origin;
  player_info* challenger = rejecter->opponent;
  if (challenger == NULL) {
    fprintf(stderr, "Malformed job on queue, type REJECT. Origin: %s",
            rejecter->name);
  } else {
    send_notice(challenger, "%s has rejected your challenge.", rejecter->name);
    rejecter->duel_status = DUEL_NONE;
    challenger->duel_status = DUEL_NONE;
  }
}

const char* determine_winner(player_info* p1, player_info* p2) {
  const char* choice1 = p1->choice;
  const char* choice2 = p2->choice;

  if (strcmp(choice1, choice2) == 0) {
    return "Nobody";  // TODO: I hate this
  }

  if ((strcmp(choice1, "ROCK") == 0 && strcmp(choice2, "SCISSORS") == 0) ||
      (strcmp(choice1, "PAPER") == 0 && strcmp(choice2, "ROCK") == 0) ||
      (strcmp(choice1, "SCISSORS") == 0 && strcmp(choice2, "PAPER") == 0)) {
    return p1->name;
  } else {
    return p2->name;
  }
}

static void handle_job_choice(job* job) {
  player_info* p1 = job->origin;
  player_info* p2 = p1->opponent;

  if (p1->choice == NULL || p2->choice == NULL) {
    return;  // only one player has submitted a choice, so leave
  }

  const char* winner = determine_winner(p1, p2);
  send_notice(p1, "Result of your duel with %s: %s wins!", p1->opponent,
              winner);
  send_notice(p2, "Result of your duel with %s: %s wins!", p2->opponent,
              winner);
  p1->choice = NULL;
  p1->duel_status = DUEL_NONE;
  p2->choice = NULL;
  p2->duel_status = DUEL_NONE;
}

static void handle_job_broadcast(job* job) {
  // iterate over all players. if they are in the same arena, send a MSG
  player_info* from = job->origin;
  int size = playerlist_getsize();
  for (size_t i = 0; i < size; i++) {
    player_info* curr = playerlist_get(i);
    if (curr->in_room == from->in_room && from != curr) {
      send_notice(curr, "From %s: %s", from->name, job->content);
    }
  }
}

/************************
 * End the notification manager.
 */
static void notif_destroy() {
  int pret = 0;
  if ((pret = pthread_detach(pthread_self())) != 0) {
    perror("pthread detach notif manager");
    exit(1);
  }

  pthread_exit(NULL);
}

/****************************
 * Start up the notification manager. Assumes jobqueue has already been
 * initialized.
 */
void* notif_main(void*) {
  notif_loop();
  notif_destroy();
  return NULL;  // so compiler doesn't complain. unreachable.
}