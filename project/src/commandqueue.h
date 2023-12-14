#ifndef COMMAND_QUEUE_H
#define COMMAND_QUEUE_H

#include <stddef.h>
#include <pthread.h>
#include "constants.h"
#include "parser.h"

typedef struct {
  enum Command cmd;
  unsigned int event_id, delay;
  int thread_id;
  size_t num_rows, num_columns, num_coords;
  size_t xs[MAX_RESERVATION_SIZE], ys[MAX_RESERVATION_SIZE];
} command;

typedef struct queueNode {
  command cmd;
  struct queueNode *next;
} QueueNode;

typedef struct {
  QueueNode *head;
  QueueNode *tail;
  pthread_mutex_t mutex;
  pthread_cond_t cond;
  pthread_cond_t barrier_cond;
  int fd;
  bool terminate;  // Flag to signal termination
  bool barrier_active;
  unsigned int *thread_wait;
} CommandQueue;

void init_queue(CommandQueue *queue, int fd, size_t max_threads);
void enqueue(CommandQueue *queue, command *cmd);
command dequeue(CommandQueue *queue);
void free_queue(CommandQueue *queue);

#endif  // COMMAND_QUEUE_H