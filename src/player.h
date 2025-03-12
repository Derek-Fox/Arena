#ifndef _PLAYER_H
#define _PLAYER_H

#include <pthread.h>
#include <stdio.h>

// The maximum length of a player name
#define PLAYER_MAXNAME 20

// These are the valid states of a player.
typedef enum player_state {
  PLAYER_UNREG,
  PLAYER_REG,
  PLAYER_DONE,
} player_state;

// Possible states for a player's duel status
typedef enum duel_status {
  DUEL_NONE,
  DUEL_PENDING,
  DUEL_ACTIVE,
} duel_status;

// The struct to keep track of all information about a player in
// the system.
typedef struct player_info player_info; // forward declaration so it can have a pointer to itself

struct player_info {
  char name[PLAYER_MAXNAME + 1];
  player_state state;
  duel_status duel_status;
  const char *choice; // Latest duel choice - meaningless if duel_status not DUEL_ACTIVE
  player_info *opponent;  // pointer to challenger - meaningless if duel_status DUEL_NONE
  int in_room;
  FILE *fp_send;
  FILE *fp_recv;
}; 

// Basic allocation/initializer and destructor functions

void player_init(player_info* player, FILE* fp_send, FILE* fp_recv);
player_info* new_player(int comm_fd);
void player_destroy(void* player);

#endif  // _PLAYER_H
