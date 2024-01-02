#include "prod_cons_queue.h"

#include <stdio.h>

void init_queue(ClientQueue *queue) {
  queue->head = queue->tail = NULL;
  pthread_mutex_init(&queue->mutex, NULL);
  pthread_cond_init(&queue->cond, NULL);
}

void enqueue(ClientQueue *queue, Client *client) {  
  pthread_mutex_lock(&queue->mutex);

  QueueNode *newNode = (QueueNode*)malloc(sizeof(QueueNode));
  if (newNode == NULL) {
    fprintf(stderr, "Error: Memory allocation failed\n");
    return;
  }

  newNode->client = *client;
  newNode->next = NULL;

  if (queue->tail == NULL) {
    queue->head = queue->tail = newNode;
  } else {
    queue->tail->next = newNode;
    queue->tail = newNode;
  }

  pthread_cond_signal(&queue->cond);
  pthread_mutex_unlock(&queue->mutex);
}

Client dequeue(ClientQueue *queue) {
  pthread_mutex_lock(&queue->mutex);

  while (queue->head == NULL) {
    // Wait for a command to be enqueued
    pthread_cond_wait(&queue->cond, &queue->mutex);
  }
  Client client;// = {.cmd = CMD_EMPTY};
  
  if (queue->head != NULL) {
    QueueNode *temp = queue->head;
    Client client = temp->client;

    queue->head = temp->next;
    if (queue->head == NULL) {
      queue->tail = NULL;
    }

    free(temp);
  }

  pthread_mutex_unlock(&queue->mutex);
  return client;
}

void free_queue(ClientQueue *queue) {
  if (!queue) return;
  QueueNode* current = queue->head;
  while (current) {
    QueueNode* temp = current;
    current = current->next;

    free(temp);
  }
  pthread_mutex_destroy(&queue->mutex);
  pthread_cond_destroy(&queue->cond);
}