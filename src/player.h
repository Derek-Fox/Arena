#ifndef _PLAYER_H
#define _PLAYER_H

#include <pthread.h>
#include <stdio.h>

// The maximum length of a player name
#define PLAYER_MAXNAME 20

// These are the valid states of a player.
#define PLAYER_UNREG 0
#define PLAYER_REG 1
#define PLAYER_DONE 2

// Possible states for a player's duel status
typedef enum {
  DUEL_NONE,
  DUEL_PENDING,
  DUEL_ACTIVE,
} DuelStatus;

// The struct to keep track of all information about a player in
// the system.

typedef struct player_info {
  char name[PLAYER_MAXNAME + 1];
  int state;
  int power;
  DuelStatus duel_status;
  char* choice; // Latest duel choice - meaningless if duel_status not DUEL_ACTIVE
  char* opponent_name;  // name of challenger - meaningless if duel_status DUEL_NONE
  int in_room;
  FILE* fp_send;
  FILE* fp_recv;
} player_info;

// Basic allocation/initializer and destructor functions

void player_init(player_info* player, FILE* fp_send, FILE* fp_recv);
player_info* new_player(int comm_fd);
void player_destroy(void* player);

#endif  // _PLAYER_H
