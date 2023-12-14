#include "eventlist.h"

#include <stdlib.h>
#include <stdio.h> //TIRAR

struct EventList* create_list() {
  struct EventList* list = (struct EventList*)malloc(sizeof(struct EventList));
  if (!list) return NULL; 
  list->head = NULL;
  list->tail = NULL;
  pthread_rwlock_init(&list->rwl, NULL);
  return list;
}

int append_to_list(struct EventList* list, struct Event* event) {
  pthread_rwlock_rdlock(&list->rwl);
  if (!list) {
    pthread_rwlock_unlock(&list->rwl);
    return 1;
  }
  pthread_rwlock_unlock(&list->rwl);

  struct ListNode* new_node = (struct ListNode*)malloc(sizeof(struct ListNode));
  if (!new_node) return 1;

  new_node->event = event;
  new_node->next = NULL;

  pthread_rwlock_wrlock(&list->rwl);
  if (list->head == NULL) {
    list->head = new_node;
    list->tail = new_node;
  } else {
    list->tail->next = new_node;
    list->tail = new_node;
  }
  printf("CREATE | Event: %u\n", event->id);
  pthread_rwlock_unlock(&list->rwl);

  return 0;
}

static void free_event(struct Event* event) {
  if (!event) return;

  pthread_rwlock_destroy(&event->rwl);
  free(event->data);
  free(event);
}

void free_list(struct EventList* list) {
  if (!list) return;

  struct ListNode* current = list->head;
  while (current) {
    struct ListNode* temp = current;
    current = current->next;

    free_event(temp->event);
    free(temp);
  }
  pthread_rwlock_destroy(&list->rwl);
  free(list);
}

struct Event* get_event(struct ListNode* head, struct ListNode* tail/*struct EventList* list*/, unsigned int event_id) {
  //if (!list) return NULL;
  if (!head) return NULL;

  struct ListNode* current = head; /*list->head;*/
  struct Event* event;
  while (current != tail->next) {
    event = current->event;
    pthread_rwlock_rdlock(&event->rwl);
    if (event->id == event_id) {
      pthread_rwlock_unlock(&event->rwl);
      return event;
    }
    pthread_rwlock_unlock(&event->rwl);
    current = current->next;
  }
  return NULL;
}
