#include <pthread.h>

#include "player.h"
#include "util.h"

#ifndef _QUEUE_H
#define _QUEUE_H

typedef enum job_type {
  JOB_DONE,
  JOB_MSG,
  JOB_JOIN,
  JOB_LEAVE,
  JOB_CHALLENGE,
  JOB_ACCEPT,
  JOB_REJECT,
  JOB_CHOICE,
  JOB_BROADCAST,
} job_type;

// Data types and function prototypes for a queue of jobs structure

// Typedef for queue, featuring blocking condition variable and lock
typedef struct queue {
  pthread_mutex_t lock;
  pthread_cond_t waiter;
  struct node* first;
  struct node* last;
} queue;

/************************************************************************
 * Typedef for jobs, for the notification manager.
 * type: job_type.
 * to: if MSG, playername of recipient. if JOIN/LEAVE, room number that
 * should receive this notification. if challenge, playername of the target.
 * content: if MSG, content of message to be sent.
 * origin: for all types, playername who issued this job.
 */
typedef struct job {
  job_type type;
  union {
    char* player_name;
    int room;
  } to;
  char* content;
  player_info* origin;
} job;

void queue_init();
void queue_enqueue(job* job);
job* queue_front();
job* queue_dequeue_wait();
void queue_destroy();

job* newjob(job_type type, void* to, char* content, player_info* origin);
void destroyjob(job* job);

#endif
