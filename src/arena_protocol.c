/* Module to implement the arena application-layer protocol.
 * The protocol is fully defined in the README file. This module
 * includes functions to parse and perform commands sent by a
 * player (the docommand function), and has functions to send
 * responses to ensure proper and consistent formatting of these
 * messages.
 * This gives access to the asprintf function - helpful, but not portable!
 */
#define _GNU_SOURCE

#include "arena_protocol.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "player.h"
#include "playerlist.h"
#include "queue.h"
#include "util.h"

/************************************************************************
 * Call this response function if an error can be described by a simple
 * string.
 */
void send_err(player_info* player, char* desc) {
    fprintf(player->fp_send, "ERR %s\n", desc);
}

/************************************************************************
 * Call this response function to send an OK followed by a description
 * string.
 */
void send_ok(player_info* player, char* desc) {
    fprintf(player->fp_send, "OK %s\n", desc);
}

/************************************************************************
 * Call this response function to send an NOTICE followed by a description
 * string.
 */
void send_notice(player_info* player, char* desc) {
    fprintf(player->fp_send, "NOTICE %s\n", desc);
}

/************************************************************************
 * Handle the "LOGIN" command. Takes one argument, a string username.
 * Sends an OK on success, or ERR if name is taken/invalid.
 * Also notifies all users in the lobby when the new player logs in.
 */
static void cmd_login(player_info* player, char* newname, char* rest) {
    if (player->state == PLAYER_REG) {  // player must not already be logged in
        char* response;
        asprintf(&response, "Already logged in as %s", player->name);
        send_err(player, response);
        free(response);
    } else if (newname == NULL) {  // player must supply a name
        send_err(player, "LOGIN missing name");
    } else if (strlen(newname) > PLAYER_MAXNAME) {  // player name cannot be too long
        char* response;
        asprintf(&response, "Invalid name -- too long (max length %d)", PLAYER_MAXNAME);
        send_err(player, response);
        free(response);
    } else if (!strisalnum(newname)) {  // player name must be alphanumeric
        send_err(player, "Invalid name -- only alphanumeric characters allowed");
    } else if (rest != NULL) {
        send_err(player, "LOGIN should have one argument");
    } else {                                                     // name valid format, but need to check if in use
        if (playerlist_changeplayername(player, newname) < 0) {  // duplicate name
            char* response;
            asprintf(&response, "Player already logged in as %s", newname);
            send_err(player, response);
            free(response);
        } else {  // finally all good
            player->state = PLAYER_REG;
            char* response;
            asprintf(&response, "Logged in as %s", newname);
            send_ok(player, response);
            free(response);

            /* Notify everyone in the lobby that player just joined. */
            job* job = newjob(JOIN, "0", NULL, player->name);
            queue_enqueue(job);
        }
    }
}

/************************************************************************
 * Handle the "MOVETO" command. Takes one argument, the arena to move to.
 * Sends an ERR on invalid input.
 * Also notifies all players in arena that player left, and players in
 * arena that player joined.
 */
static void cmd_moveto(player_info* player, char* room, char* rest) {
    char* endptr;
    int newroom = (int)strtol(room, &endptr, 0);  // convert to int --> fills endptr with first invalid character (if it exists)

    if (player->state != PLAYER_REG) {
        send_err(player, "Player must be logged in before MOVETO");
    } else if (newroom == player->in_room) {  // must move to different room
        char* response;
        asprintf(&response, "Already in arena %d", newroom);
        send_err(player, response);
        free(response);
    } else if (room == NULL || rest != NULL) {  // need 1 arg
        send_err(player, "MOVETO should have one argument");
    } else if (*endptr != '\0' || newroom < 0 || newroom > 4) {  // need valid arg
        send_err(player, "Invalid arena number");
    } else {
        char* oldroom;  // Save old room BEFORE changing it
        asprintf(&oldroom, "%d", player->in_room);

        player->in_room = newroom;

        job* job1 = newjob(JOIN, room, NULL, player->name);
        job* job2 = newjob(LEAVE, oldroom, NULL, player->name);
        free(oldroom);

        queue_enqueue(job1);
        queue_enqueue(job2);
    }
}

/************************************************************************
 * Handle the "STAT" command. Takes no arguments. Sends OK with room that
 * player is currently in.
 */
static void cmd_stat(player_info* player, char* arg1, char* rest) {
    if (player->state != PLAYER_REG) {
        send_err(player, "Player must be logged in before STAT");
    } else if (arg1 != NULL) {  // need no args
        send_err(player, "STAT should have no arguments");
    } else {  // all good
        char* response;
        asprintf(&response, "%d", player->in_room);
        send_ok(player, response);
        free(response);
    }
}

/************************************************************************
 * Handle the "LIST" command. Takes no arguments. Sends OK with list of
 * players in the current arena.
 */
static void cmd_list(player_info* player, char* arg1, char* rest) {
    if (player->state != PLAYER_REG) {
        send_err(player, "Player must be logged in before LIST");
    } else if (arg1 != NULL) {  // need no args
        send_err(player, "LIST should have no arguments");
    } else {  // all good
        int size = playerlist_getsize();
        char* response = strdup("");  // initialize response with an empty string
        /* for each player in list, if in same room, append to response string */
        for (size_t i = 0; i < size; i++) {
            player_info* curr = playerlist_get(i);
            if (curr->in_room == player->in_room) {
                char* temp;
                asprintf(&temp, "%s%s,", response, curr->name);
                free(response);   // free the old response
                response = temp;  // assign the new string to response
            }
        }
        response[strlen(response) - 1] = '\0';  // remove trailing comma
        send_ok(player, response);
        free(response);  // free the final response string
    }
}

/************************************************************************
 * Handle the "MSG" command. Takes two arguments, the target and the
 * message to send. Sends OK on success, as well as notifying target with
 * the sent message.
 */
static void cmd_msg(player_info* player, char* target, char* msg) {
    if (player->state != PLAYER_REG) {
        send_err(player, "Player must be logged in before MSG");
    } else if (target == NULL || msg == NULL) {
        send_err(player, "MSG should have 2 arguments");
    } else {
        send_ok(player, "");
        job* job = newjob(MSG, target, msg, player->name);
        queue_enqueue(job);
    }
}

/*******************************************************************
 * Handle the "BROADCAST" command. Takes one argument, the message to
 * broadcast. Sends OK on success, as well as notifying all players in
 * the same room with the broadcasted message.
 */
static void cmd_broadcast(player_info* player, char* msg, char* rest) {
    if (player->state != PLAYER_REG) {
        send_err(player, "Player must be logged in before BROADCAST");
    } else if (msg == NULL) {
        send_err(player, "BROADCAST should have an message");
    } else {
        send_ok(player, "");

        // concat msg and rest, if rest is not null. otherwise, just use msg
        char* newmsg;
        if (rest != NULL) {
            asprintf(&newmsg, "%s %s", msg, rest);
            msg = newmsg;
        } else {
            newmsg = msg;
        }

        // iterate over all players. if they are in the same arena, send a MSG
        int size = playerlist_getsize();
        for (size_t i = 0; i < size; i++) {
            player_info* curr = playerlist_get(i);
            if (curr->in_room == player->in_room) {
                job* job = newjob(MSG, curr->name, newmsg, player->name);
                queue_enqueue(job);
            }
        }

        if (rest != NULL) {
            free(newmsg);
        }
    }
}

/************************************************************************
 * Handle the "BYE" command. Takes no arguments, but not picky. Sends OK
 * on success, and unregisters the player (which will result in them being
 * removed from the playerlist and their session ending).
 */
static void cmd_bye(player_info* player, char* arg1, char* rest) {
    send_ok(player, "");
    player->state = PLAYER_DONE;
}

/************************************************************************
 * Handle the "HELP" command. Takes one optional argument, the command to
 * get help on. If no argument, lists all commands. If argument, gives help
 * on that command.
 */
static void cmd_help(player_info* player, char* cmd, char* rest) {
    if (cmd == NULL) {
        send_notice(player, "Commands: LOGIN, MOVETO, BYE, MSG, STAT, LIST, BROADCAST, HELP");
    } else {
        if (strcmp(cmd, "LOGIN") == 0) {
            send_notice(player, "LOGIN <name> - log in with a name");
        } else if (strcmp(cmd, "MOVETO") == 0) {
            send_notice(player, "MOVETO <arena> - move to a different arena");
        } else if (strcmp(cmd, "BYE") == 0) {
            send_notice(player, "BYE - log out and exit the game");
        } else if (strcmp(cmd, "MSG") == 0) {
            send_notice(player, "MSG <target> <message> - send a message to another player");
        } else if (strcmp(cmd, "STAT") == 0) {
            send_notice(player, "STAT - get the current arena number");
        } else if (strcmp(cmd, "LIST") == 0) {
            send_notice(player, "LIST - list all players in the current arena");
        } else if (strcmp(cmd, "BROADCAST") == 0) {
            send_notice(player, "BROADCAST <message> - send a message to all players in the current arena");
        } else if (strcmp(cmd, "HELP") == 0) {
            send_notice(player, "HELP [command] - get help on a command, or list all commands");
        } else {
            send_err(player, "Unknown command");
        }
    }
}

/************************************************************************
 * Parses and performs the actions in the line of text (command and
 * optionally arguments) passed in as "command".
 */
void docommand(player_info* player, char* command) {
    char* saveptr;
    char* cmd = strtok_r(command, " \t\r\n", &saveptr);
    if (cmd == NULL) {  // Empty line (no command) -- just ignore line
        return;
    }

    // Get first argument (if there is one)
    char* arg1 = strtok_r(NULL, " \r\n", &saveptr);
    if (arg1 != NULL) {
        // Don't consider an empty string an argument....
        if (arg1[0] == '\0')
            arg1 = NULL;
    }

    // Get the rest (if present -- trimmed)
    char* rest = NULL;
    if (arg1 != NULL) {
        rest = strtok_r(NULL, "\r\n", &saveptr);
        if (rest != NULL) {
            rest = trim(rest);
            // Don't consider an empty string an argument....
            if (rest[0] == '\0')
                rest = NULL;
        }
    }

    /* Parsing result: "cmd" has the command string, "arg1" has the
     * next word after "cmd" if it exists (NULL if not), and "rest"
     * has the rest of line after the first argument (NULL if not
     * present).
     */

    if (strcmp(cmd, "LOGIN") == 0) {
        cmd_login(player, arg1, rest);
    } else if (strcmp(cmd, "MOVETO") == 0) {
        cmd_moveto(player, arg1, rest);
    } else if (strcmp(cmd, "BYE") == 0) {
        cmd_bye(player, arg1, rest);
    } else if (strcmp(cmd, "MSG") == 0) {
        cmd_msg(player, arg1, rest);
    } else if (strcmp(cmd, "STAT") == 0) {
        cmd_stat(player, arg1, rest);
    } else if (strcmp(cmd, "LIST") == 0) {
        cmd_list(player, arg1, rest);
    } else if (strcmp(cmd, "BROADCAST") == 0) {
        cmd_broadcast(player, arg1, rest);
    } else if (strcmp(cmd, "HELP") == 0) {
        cmd_help(player, arg1, rest);
    } else {
        send_err(player, "Unknown command");
    }
}
