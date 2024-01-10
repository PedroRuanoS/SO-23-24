#ifndef PROD_CONS_QUEUE_H
#define PROD_CONS_QUEUE_H

#include <stddef.h>
#include <stdlib.h>
#include <pthread.h>

typedef struct {
  char *req_pipe_path;
  char *resp_pipe_path;
} Client;

typedef struct queueNode {
  Client client;
  struct queueNode *next;
} QueueNode;

// Linked list structure
typedef struct {
  QueueNode *head;              // Head of the list
  QueueNode *tail;              // Tail of the list
  pthread_mutex_t mutex;        // Queue mutex
  pthread_cond_t cond;          // Condition variable
} ClientQueue;   // FIFO queue

/// Initializes a queue of clients.
/// @param queue to be initialized.
void init_queue(ClientQueue *queue);

/// Adds a client to the queue.
/// @param queue client queue to be modified.
/// @param client client info.
void enqueue(ClientQueue *queue, Client *client);

/// Retrieves a client from the queue.
/// @param queue client queue to be modified.
/// @return client info.
Client dequeue(ClientQueue *queue);

/// Frees all memory allocated for the queue and destroys its mutex and condition variables.
/// @param queue to be freed.
void free_queue(ClientQueue *queue);

#endif  // PROD_CONS_QUEUE_H