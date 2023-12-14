#ifndef COMMAND_QUEUE_H
#define COMMAND_QUEUE_H

#include <stddef.h>
#include <pthread.h>
#include <stdbool.h>
#include "constants.h"
#include "parser.h"

typedef struct {
  enum Command cmd;                     // Command to be executed.
  unsigned int event_id, delay;         // Event id and delay for this thread.
  int thread_id;                        // Thread id.
  size_t num_rows, num_columns, num_coords;              // Number of rows, number of columns and number of seats to reserve.
  size_t xs[MAX_RESERVATION_SIZE], ys[MAX_RESERVATION_SIZE];    // Arrays of rows and of columns of the seats to reserve, respectively.
} CommandArgs;

typedef struct queueNode {
  CommandArgs cmd;
  struct queueNode *next;
} QueueNode;

// Linked list structure
typedef struct {
  QueueNode *head;              // Head of the list
  QueueNode *tail;              // Tail of the list
  pthread_mutex_t mutex;        // Queue mutex
  pthread_cond_t cond;          // 
  pthread_cond_t barrier_cond;  //
  int fd;                       // File descriptor to write in.
  bool terminate;  // Flag to signal termination
  bool barrier_active;
  unsigned int *thread_wait;
} CommandQueue;

void init_queue(CommandQueue *queue, int fd, size_t max_threads);
void enqueue(CommandQueue *queue, CommandArgs *cmd);
CommandArgs dequeue(CommandQueue *queue);
void free_queue(CommandQueue *queue);

#endif  // COMMAND_QUEUE_H