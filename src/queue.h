#include <pthread.h>

#include "player.h"
#include "util.h"

#ifndef _QUEUE_H
#define _QUEUE_H

typedef enum {
  DONE,
  MSG,
  JOIN,
  LEAVE,
  CHALLENGE,
  ACCEPT,
  REJECT,
  RESULT,
} JobType;

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
 * type: JobType.
 * to: if MSG, playername of recipient. if JOIN/LEAVE, room number that
 * should receive this notification.
 * content: if MSG, content of message to be sent. if JOIN/LEAVE, NULL.
 * origin: for all types, playername who issued this job.
 */
typedef struct job {
  JobType type;
  char* to;
  char* content;
  char* origin;
} job;

void queue_init();
void queue_enqueue(job* job);
job* queue_front();
job* queue_dequeue_wait();
void queue_destroy();

job* newjob(JobType type, char* to, char* content, char* origin);
void destroyjob(job* job);

#endif
