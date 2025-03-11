#include "duel.h"

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include "player.h"
#include "playerlist.h"

#define MIN_POWER 1
#define MAX_POWER 100

int execute_duel(player_info* player1, player_info* player2) {
  char *lineptr = NULL;
  size_t linesize = 0;
  getline(&lineptr, &linesize, player1->fp_recv);
  fprintf(player1->fp_send, "You chose %s", lineptr);
  free(lineptr);
  return 1;
}

/************************************************************
 * Award power to the winner of the duel, and take power from the loser.
 * Ensure player's power levels stay within bounds. 
 */
void award_power(player_info* winner, player_info* loser) {
  if (winner->power < MAX_POWER) winner->power++;
  if (loser->power > MIN_POWER) loser->power--;
}