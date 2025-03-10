// The player module contains the player data type and management functions

#include "player.h"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/************************************************************************
 * player_init initializes an player structure in the initial PLAYER_UNREG
 * state, with given send and receive FILE objects.
 */
void player_init(player_info *player, FILE *fp_send, FILE *fp_recv) {
  player->name[0] = '\0';
  player->state = PLAYER_UNREG;
  player->power = 1;
  player->challenge_pending = false;
  player->challenge_from = strdup("");
  player->in_room = 0;
  player->fp_send = fp_send;
  player->fp_recv = fp_recv;
}

/************************************************************************
 * new_player returns a pointer to fully initialized player with correct
 * read/write file handles and memory allocated to it.
 */
player_info *new_player(int comm_fd) {
  /* Set up file descriptors for read/write */
  int infd = dup(comm_fd);
  FILE *player_in = NULL;
  FILE *player_out = NULL;
  player_in = fdopen(infd, "r");
  player_out = fdopen(comm_fd, "w");
  setvbuf(player_in, NULL, _IOLBF, 0);
  setvbuf(player_out, NULL, _IOLBF, 0);

  /* Set up player to handle and add to global playerlist */
  player_info *player = NULL;
  if ((player = malloc(sizeof(player_info))) == NULL) {
    perror("player malloc");
    exit(1);
  }
  player_init(player, player_out, player_in);

  return player;
}

/************************************************************************
 * player_destroy frees up any resources associated with a player, like
 * file handles, so that it can be free'ed.
 */
void player_destroy(void *player) {
  ((player_info *)player)->state = PLAYER_DONE;  // Just to make sure....
  fclose(((player_info *)player)->fp_send);
  fclose(((player_info *)player)->fp_recv);
  free(((player_info *)player)->challenge_from);
}
