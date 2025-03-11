#ifndef _DUEL_H
#define _DUEL_H

#include "player.h"

int execute_duel(player_info* player1, player_info* player2);
void award_power(player_info* winner, player_info* loser);

#endif
