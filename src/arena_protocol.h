// Defines the publicly-callable functions in the arena_commands module

#ifndef _ARENA_COMMANDS_H
#define _ARENA_COMMANDS_H

#include "player.h"

void send_notice(player_info* player, char* desc);
void docommand(player_info* player, char* command);

#endif  // _ARENA_COMMANDS_H
