// Module which manages the global playerlist structure. Uses underlying generic alist struct.


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#include "playerlist.h"
#include "alist.h"
#include "player.h"

playerlist* global_plist;

/* Initializes the list of players */
void playerlist_init() {
    if((global_plist = malloc(sizeof(playerlist))) == NULL) {
        perror("malloc global_list");
        exit(1);
    }
    alist* parrlist = NULL;
    if ((parrlist = malloc(sizeof(alist))) == NULL) {
        perror("malloc parrlist");
        exit(1);
    }

    alist_init(parrlist, player_destroy);
    global_plist->parrlist = parrlist;

    pthread_rwlock_init(&(global_plist->lock), NULL);
}

/* Returns the number of players in the list */
int playerlist_getsize() {
    int retval = 0;
    pthread_rwlock_rdlock(&global_plist->lock);
    retval = global_plist->parrlist->in_use;
    pthread_rwlock_unlock(&global_plist->lock);
    return retval;
}

/* Adds a player to the player list */
void playerlist_addplayer(player_info* player) {
    pthread_rwlock_wrlock(&global_plist->lock);
    alist_add(global_plist->parrlist, player);
    pthread_rwlock_unlock(&global_plist->lock);
}

/* Removes a player from the player list */
void playerlist_removeplayer(player_info* player) {
    player_info* curr = NULL;
    pthread_rwlock_wrlock(&global_plist->lock);
    for (size_t i = 0; i < global_plist->parrlist->in_use; i++) {
        curr = (player_info*)alist_get(global_plist->parrlist, i);
        if (!strcmp(curr->name, player->name)) {
            alist_remove(global_plist->parrlist, i);
        }
    }
    pthread_rwlock_unlock(&global_plist->lock);
}

/* Returns the corresponding player struct, given a name. Returns NULL if player not found. */
player_info* playerlist_findplayer(char* name) {
    player_info* curr = NULL;

    pthread_rwlock_rdlock(&global_plist->lock);
    for (size_t i = 0; i < global_plist->parrlist->in_use; i++) {
        curr = (player_info*)alist_get(global_plist->parrlist, i);
        if (!strcmp(curr->name, name)) {
            pthread_rwlock_unlock(&global_plist->lock);
            return curr;
        }
    }

    pthread_rwlock_unlock(&global_plist->lock);
    return NULL;
}

/* Return player at index i */
player_info* playerlist_get(int i) {
    player_info* retval = NULL;
    pthread_rwlock_rdlock(&global_plist->lock);
    retval = (player_info*)alist_get(global_plist->parrlist, i);
    pthread_rwlock_unlock(&global_plist->lock);
    return retval;
}

/* Changes the name of the given player to given new name. Returns negative value if name is already in use. */
int playerlist_changeplayername(player_info* player, char* name) {
    if (playerlist_findplayer(name) != NULL) {  // duplicate name found
        return -1;
    }
    pthread_rwlock_wrlock(&global_plist->lock);
    strcpy(player->name, name);
    pthread_rwlock_unlock(&global_plist->lock);
    return 0;
}

/* Frees all resources used by the given playerlist. Frees space allocated for playerlist, so all operations on it afterwards are illegal! */
void playerlist_destroy() {
    alist_destroy(global_plist->parrlist);
    free(global_plist->parrlist);
    pthread_rwlock_destroy(&global_plist->lock);
    free(global_plist);
}