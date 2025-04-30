// Defines the publicly-callable functions in the arena_commands module

#ifndef _ARENA_COMMANDS_H
#define _ARENA_COMMANDS_H

#define ROOM_LOBBY 0

#include "player.h"

void send_notice(player_info* player, const char* format, ...);
void send_err(player_info* player, const char* format, ...);
void docommand(player_info* player, char* command);

#endif  // _ARENA_COMMANDS_H
