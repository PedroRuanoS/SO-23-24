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
  CommandArgs cmd_args;
  struct queueNode *next;
} QueueNode;

// Linked list structure
typedef struct {
  QueueNode *head;              // Head of the list
  QueueNode *tail;              // Tail of the list
  pthread_mutex_t mutex;        // Queue mutex
  pthread_cond_t cond;          // Condition variable
  pthread_cond_t barrier_cond;  // Condition variable for BARRIER commmands.
  int fd;                       // File descriptor to write in.
  bool terminate;               // Flag to signal termination (No more commands to parse).
  bool barrier_active;          // Flag to signal activation of a barrier.
  unsigned int *thread_wait;    // Array of delays for each thread.
} CommandQueue;   // FIFO queue

/// Initializes a queue of commands.
/// @param queue to be initialized.
/// @param fd File descriptor to write in.
/// @param max_threads Maximum number of threads.
void init_queue(CommandQueue *queue, int fd, size_t max_threads);

/// Adds a command to the queue.
/// @param queue Command queue to be modified.
/// @param cmd Command arguments.
void enqueue(CommandQueue *queue, CommandArgs *cmd_args);

/// Retrieves a command from the queue.
/// @param queue Command queue to be modified.
/// @return Command arguments.
CommandArgs dequeue(CommandQueue *queue);

/// Frees all memory allocated for the queue and destroys its mutex and condition variables.
/// @param queue to be freed.
void free_queue(CommandQueue *queue);

#endif  // COMMAND_QUEUE_H