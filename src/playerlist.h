// Function prototypes and typedef for playerlist
#ifndef _PLYLIST_H
#define _PLYLIST_H

#include <pthread.h>

#include "alist.h"
#include "player.h"

typedef struct {
  alist* parrlist;
  pthread_rwlock_t lock;
} playerlist;

void playerlist_init();
int playerlist_getsize();
void playerlist_addplayer(player_info* player);
void playerlist_removeplayer(player_info* player);
player_info* playerlist_findplayer(char* name);
player_info* playerlist_get(int i);
int playerlist_changeplayername(player_info* player, char* name);
void playerlist_destroy();
#endif