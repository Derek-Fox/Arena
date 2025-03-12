/* Module to implement the arena application-layer protocol.
 * The protocol is fully defined in the README file. This module
 * includes functions to parse and perform commands sent by a
 * player (the docommand function), and has functions to send
 * responses to ensure proper and consistent formatting of these
 * messages.
 */
#define MAX_RESPONSE_LEN 256
#define MAX_MSG_LEN 200

#include "arena_protocol.h"

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "player.h"
#include "playerlist.h"
#include "queue.h"
#include "util.h"

/************************************************************************
 * Helper function to send a response with a specified type and format string
 * with optional args.
 */
static void send_response(player_info* player, const char* type,
                          const char* format, va_list args) {
  char response[MAX_RESPONSE_LEN];
  vsnprintf(response, MAX_RESPONSE_LEN, format, args);
  fprintf(player->fp_send, "%s %s\n", type, response);
}

/************************************************************************
 * Response function to send an ERR followed by format string with optional
 * args.
 */
void send_err(player_info* player, const char* format, ...) {
  va_list args;
  va_start(args, format);
  send_response(player, "ERR", format, args);
  va_end(args);
}

/************************************************************************
 * Call this response function to send an OK followed by a format string
 * with optional args.
 */
void send_ok(player_info* player, const char* format, ...) {
  va_list args;
  va_start(args, format);
  send_response(player, "OK", format, args);
  va_end(args);
}

/************************************************************************
 * Call this response function to send a NOTICE followed by a format string
 * with optional args.
 */
void send_notice(player_info* player, const char* format, ...) {
  va_list args;
  va_start(args, format);
  send_response(player, "NOTICE", format, args);
  va_end(args);
}

/************************************************************************
 * Handle the "LOGIN" command. Takes one argument, a string username.
 * Sends an OK on success, or ERR if name is taken/invalid.
 * Also notifies all users in the lobby when the new player logs in.
 */
static void cmd_login(player_info* player, char* newname, char* rest) {
  if (player->state == PLAYER_REG) {  // player must not already be logged in
    send_err(player, "Already logged in as %s", player->name);
  } else if (newname == NULL) {  // player must supply a name
    send_err(player, "LOGIN missing name");
  } else if (strlen(newname) >
             PLAYER_MAXNAME) {  // player name cannot be too long
    send_err(player, "Invalid name -- too long (max length %d)",
             PLAYER_MAXNAME);
  } else if (!strisalnum(newname)) {  // player name must be alphanumeric
    send_err(player, "Invalid name -- only alphanumeric characters allowed");
  } else if (rest != NULL) {
    send_err(player, "LOGIN should have one argument");
  } else {  // name valid format, but need to check if in use
    if (playerlist_changeplayername(player, newname) < 0) {  // duplicate name
      send_err(player, "Player already logged in as %s", newname);
    } else {  // finally all good
      player->state = PLAYER_REG;
      send_ok(player, "Logged in as %s", newname);

      /* Notify everyone in the lobby that player just joined. */
      int lobby = 0;
      job* job = newjob(JOB_JOIN, &lobby, NULL, player);
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
  int newroom = (int)strtol(room, &endptr, 0);  // convert to int
  if (player->state != PLAYER_REG) {
    send_err(player, "Player must be logged in before MOVETO");
  } else if (newroom == player->in_room) {  // must move to different room
    send_err(player, "Already in arena %d", newroom);
  } else if (room == NULL || rest != NULL) {  // need 1 arg
    send_err(player, "MOVETO should have one argument");
  } else if (*endptr != '\0' || newroom < 0 || newroom > 4) {  // need valid arg
    send_err(player, "Invalid arena number");
  } else {
    int oldroom = player->in_room;  // save old room before changing it
    player->in_room = newroom;

    job* job1 = newjob(JOB_JOIN, &newroom, NULL, player);
    job* job2 = newjob(JOB_LEAVE, &oldroom, NULL, player);

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
    if (player->in_room == 0) {
      send_ok(player, "lobby");
    } else {
      send_ok(player, "%d", player->in_room);
    }
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
    size_t response_size = 1;  // start with 1 for the null terminator
    char* response =
        malloc(response_size);  // initialize response with an empty string
    if (response == NULL) {
      perror("malloc LIST");
      return;
    }

    response[0] = '\0';  // initialize as empty string

    /* for each player in list, if in same room, append to response string */
    for (size_t i = 0; i < size; i++) {
      player_info* curr = playerlist_get(i);
      if (curr->in_room == player->in_room) {
        size_t name_len = strlen(curr->name);
        response_size += name_len + 1;  // +1 for the comma
        response = realloc(response, response_size);
        if (response == NULL) {
          perror("realloc LIST");
          return;
        }
        strcat(response, curr->name);
        strcat(response, ",");
      }
    }

    if (response[0] != '\0') {
      response[strlen(response) - 1] = '\0';  // remove trailing comma
    }

    send_ok(player, "%s", response);
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
  } else if (strlen(msg) > MAX_MSG_LEN) {
    send_err(player, "Message too long. Max length is %d", MAX_MSG_LEN);
  } else {
    send_ok(player, "");
    job* job = newjob(JOB_MSG, target, msg, player);
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
    send_err(player, "BROADCAST should have a message");
  } else {
    /* Need to combine msg and rest, as command is given BROADCAST <msg which
    might include spaces> but is parsed into <cmd (BROADCAST)> <first word of
    msg> <rest of msg> */
    // Calculate the length of the new message
    size_t rest_len =
        (rest != NULL) ? strlen(rest) + 1  // +1 for space between msg and rest
                       : 0;
    size_t newmsg_len = strlen(msg) + rest_len + 1;  // +1 for null terminator

    if (newmsg_len > MAX_MSG_LEN) {
      send_err(player, "Message too long. Max length is %d", MAX_MSG_LEN);
      return;
    }

    // Allocate memory for the new message
    char* newmsg = malloc(newmsg_len);
    if (newmsg == NULL) {
      perror("malloc");
      return;
    }

    // Copy msg to newmsg and concatenate rest if it is not NULL
    strcpy(newmsg, msg);
    if (rest != NULL) {
      strcat(newmsg, " ");
      strcat(newmsg, rest);
    }

    send_ok(player, "");
    // iterate over all players. if they are in the same arena, send a MSG
    int size = playerlist_getsize();
    for (size_t i = 0; i < size; i++) {
      player_info* curr = playerlist_get(i);
      if (curr->in_room == player->in_room) {
        job* job = newjob(JOB_MSG, curr->name, newmsg, player);
        queue_enqueue(job);
      }
    }

    free(newmsg);
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
 * Handle the "WHOAMI" command. Takes no arguments. Sends OK with the
 * player's name and power level.
 */
static void cmd_whoami(player_info* player, char* arg1, char* rest) {
  if (player->state != PLAYER_REG) {
    send_err(player, "Player must be logged in before WHOAMI");
  } else if (arg1 != NULL) {  // need no args
    send_err(player, "WHOAMI should have no arguments");
  } else {  // all good
    send_ok(player, "%s: %d", player->name, player->power);
  }
}

/************************************************************************
 * Handle the "HELP" command. Takes one optional argument, the command to
 * get help on. If no argument, lists all commands. If argument, gives help
 * on that command.
 */
static void cmd_help(player_info* player, char* cmd, char* rest) {
  if (cmd == NULL) {
    send_notice(player,
                "Commands: LOGIN, MOVETO, BYE, MSG, STAT, LIST, BROADCAST, "
                "HELP, WHOAMI, CHALLENGE, ACCEPT, REJECT");
  } else {
    if (strcmp(cmd, "LOGIN") == 0) {
      send_notice(player, "LOGIN <name> - log in with a name");
    } else if (strcmp(cmd, "MOVETO") == 0) {
      send_notice(player, "MOVETO <arena> - move to a different arena");
    } else if (strcmp(cmd, "BYE") == 0) {
      send_notice(player, "BYE - log out and exit the server");
    } else if (strcmp(cmd, "MSG") == 0) {
      send_notice(player,
                  "MSG <target> <message> - send a message to another player");
    } else if (strcmp(cmd, "STAT") == 0) {
      send_notice(player, "STAT - get the current arena number");
    } else if (strcmp(cmd, "LIST") == 0) {
      send_notice(player, "LIST - list all players in the current arena");
    } else if (strcmp(cmd, "BROADCAST") == 0) {
      send_notice(player,
                  "BROADCAST <message> - send a message to all players in the "
                  "current arena");
    } else if (strcmp(cmd, "HELP") == 0) {
      send_notice(
          player,
          "HELP [command] - get help on a command, or list all commands");
    } else if (strcmp(cmd, "WHOAMI") == 0) {
      send_notice(player, "WHOAMI - get your own name and power level");
    } else if (strcmp(cmd, "CHALLENGE") == 0) {
      send_notice(player,
                  "CHALLENGE <player> - challenge another player to a duel "
                  "(the winner is decided based on each player's power level)");
    } else if (strcmp(cmd, "ACCEPT") == 0) {
      send_notice(player,
                  "ACCEPT - accept an incoming challenge from another player");
    } else if (strcmp(cmd, "REJECT") == 0) {
      send_notice(player,
                  "REJECT - reject an incoming challenge from another player");
    } else if (strcmp(cmd, "CHOOSE") == 0) {
      send_notice(
          player,
          "CHOOSE <ROCK, PAPER, SCISSORS> - choose your move during a duel.");
    } else {
      send_err(player, "Unknown command");
    }
  }
}

/****************************************
 * Handle the CHALLENGE command. Takes one argument, the player to send a
 * challenge to. Sends OK if challenge was sent successfully. Sends ERR if
 * target player does not exist.
 */
static void cmd_challenge(player_info* player, char* target, char* rest) {
  if (player->state != PLAYER_REG) {
    send_err(player, "Player must be logged in before CHALLENGE");
  } else if (target == NULL || rest != NULL) {
    send_err(player, "CHALLENGE should have one argument");
  } else if (player->duel_status == DUEL_PENDING) {
    send_err(player, "Already have pending challenge with %s",
             player->opponent->name);
  } else {
    send_ok(player, "");
    job* job = newjob(JOB_CHALLENGE, target, NULL, player);
    queue_enqueue(job);
  }
}

/***********************************************
 * Handle the ACCEPT command. Takes no arguments. If player currently has a
 * pending challenge, it starts the duel. If not, sends ERR.
 */
static void cmd_accept(player_info* player, char* arg1, char* rest) {
  if (player->state != PLAYER_REG) {
    send_err(player, "Player must be logged in before ACCEPT");
  } else if (arg1 != NULL) {
    send_err(player, "ACCEPT should have no arguments");
  } else if (player->duel_status != DUEL_PENDING) {
    send_err(player, "No challenge pending");
  } else {
    send_ok(player, "");
    job* job1 = newjob(JOB_ACCEPT, NULL, NULL, player);
    queue_enqueue(job1);
  }
}

/***********************************************
 * Handle the REJECT command. Takes no arguments. If player currently has a
 * pending challenge, it denies the duel. If not, sends ERR.
 */
static void cmd_reject(player_info* player, char* arg1, char* rest) {
  if (player->state != PLAYER_REG) {
    send_err(player, "Player must be logged in before REJECT");
  } else if (arg1 != NULL) {
    send_err(player, "REJECT should have no arguments");
  } else if (player->duel_status != DUEL_PENDING) {
    send_err(player, "No challenge pending");
  } else {
    send_ok(player, "");
    job* job = newjob(JOB_REJECT, NULL, NULL, player);
    queue_enqueue(job);
  }
}

/*********************************************************
 * Helper function to validate the choice made by the player
 * in the CHOOSE cmd.
 */
static int validate_choice(const char* choice) {
  if ((strcmp(choice, "ROCK") == 0) || (strcmp(choice, "PAPER") == 0) ||
      (strcmp(choice, "SCISSORS") == 0))
    return 1;
  else
    return 0;
}

/***************************************************
 * Handle the CHOOSE command. Takes one argument from "ROCK, PAPER, SCISSORS".
 * Sends ERR if player does not have an active duel, or if they make an invalid
 * choice.
 */
static void cmd_choose(player_info* player, char* choice, char* rest) {
  if (player->state != PLAYER_REG) {
    send_err(player, "Player must be logged in before CHOOSE");
  } else if (choice == NULL) {
    send_err(player, "CHOOSE needs one argument.");
  } else if (player->duel_status != DUEL_ACTIVE) {
    send_err(player,
             "You do not have an active duel. If you have a pending duel, they "
             "must ACCEPT.");
  } else {
    if (!validate_choice(choice)) {
      send_err(player, "Invalid choice. Choose from ROCK, PAPER, or SCISSORS.");
      return;
    }
    send_ok(player, "%s", choice);
    player->choice = choice;

    job* job = newjob(JOB_CHOICE, NULL, NULL, player);
    queue_enqueue(job);
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
    if (arg1[0] == '\0') arg1 = NULL;
  }

  // Get the rest (if present -- trimmed)
  char* rest = NULL;
  if (arg1 != NULL) {
    rest = strtok_r(NULL, "\r\n", &saveptr);
    if (rest != NULL) {
      rest = trim(rest);
      // Don't consider an empty string an argument....
      if (rest[0] == '\0') rest = NULL;
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
  } else if (strcmp(cmd, "WHOAMI") == 0) {
    cmd_whoami(player, arg1, rest);
  } else if (strcmp(cmd, "CHALLENGE") == 0) {
    cmd_challenge(player, arg1, rest);
  } else if (strcmp(cmd, "ACCEPT") == 0) {
    cmd_accept(player, arg1, rest);
  } else if (strcmp(cmd, "REJECT") == 0) {
    cmd_reject(player, arg1, rest);
  } else if (strcmp(cmd, "CHOOSE") == 0) {
    cmd_choose(player, arg1, rest);
  } else {
    send_err(player, "Unknown command");
  }
}
