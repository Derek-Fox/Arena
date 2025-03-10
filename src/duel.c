#include "duel.h"

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include "player.h"
#include "playerlist.h"

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

void award_power(player_info* winner, player_info* loser) {
  winner->power++;
  loser->power--;
  if (loser->power < 1) loser->power = 1;
}