#include "commandqueue.h"

#include <stdio.h>
#include <stdlib.h>

void init_queue(CommandQueue *queue, int fd, size_t max_threads) {
  queue->head = queue->tail = NULL;
  pthread_mutex_init(&queue->mutex, NULL);
  pthread_cond_init(&queue->cond, NULL);
  pthread_cond_init(&queue->barrier_cond, NULL);
  queue->fd = fd;
  queue->terminate = false;
  queue->barrier_active = false;
  queue->thread_wait = (unsigned int*)malloc(max_threads * sizeof(unsigned int));
  if (queue->thread_wait == NULL) {
    fprintf(stderr, "Error: Memory allocation failed\n");
    exit(1);
  }
  for (size_t i = 0; i < max_threads; i++) {
        queue->thread_wait[i] = 0;
    }
}

void enqueue(CommandQueue *queue, CommandArgs *cmd) {
  printf("Enqueue | Command: %d | Fd: %d\n", cmd->cmd, queue->fd);
  
  pthread_mutex_lock(&queue->mutex);

  QueueNode *newNode = (QueueNode*)malloc(sizeof(QueueNode));
  if (newNode == NULL) {
    fprintf(stderr, "Error: Memory allocation failed\n");
    exit(1);
  }

  newNode->cmd = *cmd;
  newNode->next = NULL;

  if (queue->tail == NULL) {
    queue->head = queue->tail = newNode;
  } else {
    queue->tail->next = newNode;
    queue->tail = newNode;
  }
///////////
  QueueNode *current = queue->head;
  while (current) {
    printf("QueueNode: %d\n", current->cmd.cmd);
    current = current->next;
  }
////////////
  pthread_cond_signal(&queue->cond);
  pthread_mutex_unlock(&queue->mutex);
}

CommandArgs dequeue(CommandQueue *queue) {
  pthread_mutex_lock(&queue->mutex);

  while (queue->head == NULL && !queue->terminate) {
    // Wait for a command to be enqueued or termination signal
    printf("Waiting for commands to be enqueued\n");
    pthread_cond_wait(&queue->cond, &queue->mutex);

  }
/////////////
  CommandArgs cmd_args = {.cmd = CMD_EMPTY};
  
  if (queue->head != NULL) {
    printf("queue head not NULL \n");

    QueueNode *temp = queue->head;
    cmd_args = temp->cmd;

    printf("cmd: %d\n", cmd_args.cmd);

    queue->head = temp->next;
    if (queue->head == NULL) {
      queue->tail = NULL;
    }

    free(temp);
  }
  else {
    printf("queue head NULL\n");
  }
  
  if (queue->head == NULL && queue->barrier_active) {
    // notify main thread once all commands preceding barrier have been executed
    pthread_cond_signal(&queue->barrier_cond);
  }

  pthread_mutex_unlock(&queue->mutex);
  return cmd_args;
}

void free_queue(CommandQueue *queue) {
  if (!queue) return;
  QueueNode* current = queue->head;
  while (current) {
    QueueNode* temp = current;
    current = current->next;

    free(temp);
  }
  free(queue->thread_wait);
  pthread_mutex_destroy(&queue->mutex);
  pthread_cond_destroy(&queue->cond);
  pthread_cond_destroy(&queue->barrier_cond);
}