#include "duel.h"

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include "player.h"
#include "playerlist.h"

#define MIN_POW 1
#define MAX_POW 100

/**********************************************************
 * Run the duel between two players. Return 1 if p1 wins, 2 if p2 wins.
 * Randomly choose a player to win based on their power level. Higher power =
 * greater chance to win.
 */
int execute_duel(player_info* player1, player_info* player2) {
  // Initialize random number generator
  srand(time(NULL));

  int total_power = player1->power + player2->power;
  int random_value = rand() % total_power;

  if (random_value < player1->power) {
    return 1;
  } else {
    return 2;
  }
}

/************************************************************
 * Award power to the winner of the duel, and take power from the loser.
 * Ensure player's power levels stay within bounds. 
 */
void award_power(player_info* winner, player_info* loser) {
  if (winner->power < MAX_POW) winner->power++;
  if (loser->power > MIN_POW) loser->power--;
}