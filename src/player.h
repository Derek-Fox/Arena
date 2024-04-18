// Contains function prototypes and typedefs for player_info struct

#ifndef _PLAYER_H
#define _PLAYER_H

#include <stdio.h>
#include <pthread.h>

// The maximum length of a player name

#define PLAYER_MAXNAME 20

// These are the valid states of a player. The numbers don't mean
// anything, and just need to be all different. Note that a more
// "modern" way of doing this would be to use an "enum", but most C
// programmers stick to this old-fashioned way of doing things - so we
// will too!

#define PLAYER_UNREG 0
#define PLAYER_REG 1
#define PLAYER_DONE 2

// The struct to keep track of all information about a player in
// the system.

typedef struct player_info {
    char name[PLAYER_MAXNAME+1];
    int state;
    int in_room;
    FILE* fp_send;
    FILE* fp_recv;
} player_info;

// Basic allocation/initializer and destructor functions

void player_init(player_info* player, FILE *fp_send, FILE *fp_recv);
player_info* new_player(int comm_fd);
void player_destroy(void* player);

#endif  // _PLAYER_H
