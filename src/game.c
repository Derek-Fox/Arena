#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>

#include "player.h"
#include "playerlist.h"
#include "game.h"

/**********************************************************
 * Run the game between two players. Return 1 if p1 wins, 2 if p2 wins.
*/
int run_game(player_info* player1, player_info* player2) {
    // randomly choose a player to win based on their power level. higher power = greater chance to win.

    sleep(1);

    fprintf(player1->fp_send, "Determining the winner...\n");
    fprintf(player2->fp_send, "Determining the winner...\n");

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