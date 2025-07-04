/* Job queue for use by notification manager.
 * Jobs on the queue contain a variety of info which is
 * discussed in the typedef for jobs, in queue.h.
 */

#include "queue.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

queue* jobq;

typedef struct node {
  job* job;
  struct node* next;
} node;

/******************************************************************
 * Initialize a queue (it starts empty)
 */
void queue_init() {
  if ((jobq = malloc(sizeof(queue))) == NULL) {
    perror("malloc");
    exit(1);
  }
  jobq->first = jobq->last = NULL;
  pthread_mutex_init(&jobq->lock, NULL);
  pthread_cond_init(&jobq->waiter, NULL);
}

/******************************************************************
 * Add a new job to the queue
 */
void queue_enqueue(job* job) {
  node* newnode = malloc(sizeof(node));
  if ((newnode == NULL)) {
    fprintf(stderr, "Out of memory! Exiting program.\n");
    exit(1);
  }

  pthread_mutex_lock(&jobq->lock);
  newnode->job = job;
  newnode->next = NULL;

  if (jobq->first == NULL)
    jobq->first = newnode;
  else
    jobq->last->next = newnode;

  jobq->last = newnode;
  pthread_cond_signal(&jobq->waiter);
  pthread_mutex_unlock(&jobq->lock);
}

/******************************************************************
 * Return the job at the front of the queue. Returns NULL if the
 * queue is empty.
 */
job* queue_front() {
  if (jobq->first == NULL) {
    return NULL;
  } else {
    return jobq->first->job;
  }
}

/******************************************************************
 * Removes the front item from the queue. Waits for queue to have
 * an item if currently empty.
 */
job* queue_dequeue_wait() {
  pthread_mutex_lock(&jobq->lock);
  while (jobq->first == NULL) {
    pthread_cond_wait(&jobq->waiter, &jobq->lock);
  }
  node* front = jobq->first;
  job* retval = front->job;
  jobq->first = jobq->first->next;
  pthread_mutex_unlock(&jobq->lock);

  free(front);
  return retval;
}

/******************************************************************
 * Destroy a queue - frees up all resources associated with the queue.
 */
void queue_destroy() {
  while (jobq->first != NULL) {
    node* curr = jobq->first;
    jobq->first = curr->next;
    destroyjob(curr->job);
    free(curr);
  }
  free(jobq);
}

/************************************************************************
 * Create a new job struct, fully allocated and initialized with desired
 * values. See struct definition for more info on fields.
 */
job* newjob(job_type type, void* to, char* content, player_info* origin) {
  job* new_job = NULL;
  if ((new_job = malloc(sizeof(job))) == NULL) {
    perror("malloc job");
    exit(1);
  }

  new_job->type = type;

  if (type == JOB_MSG || type == JOB_CHALLENGE || type == JOB_FIND) {
    new_job->to.player_name = (char*)to;
  } else if (type == JOB_JOIN || type == JOB_LEAVE) {
    new_job->to.room = *(int*)to;
  }

  if (content != NULL) {
    new_job->content = malloc(strlen(content) + 1);
    if (new_job->content == NULL) {
      perror("malloc jobcontent");
      exit(1);
    }
    strcpy(new_job->content, content);
  } else {
    new_job->content = NULL;
  }

  new_job->origin = origin;

  return new_job;
}

/************************************************************************
 * Frees all necessary fields of this job and the job itself.
 */
void destroyjob(job* job) {
  if (job->content != NULL) {
    free(job->content);
  }
  free(job);
}