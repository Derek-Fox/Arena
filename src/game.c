#include "game.h"

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include "player.h"
#include "playerlist.h"

/**********************************************************
 * Run the game between two players. Return 1 if p1 wins, 2 if p2 wins.
 * Randomly choose a player to win based on their power level. Higher power =
 * greater chance to win.
 */
int run_game(player_info* player1, player_info* player2) {
  fprintf(player1->fp_send, "Determining the winner...\n");
  fprintf(player2->fp_send, "Determining the winner...\n");

  sleep(1);
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