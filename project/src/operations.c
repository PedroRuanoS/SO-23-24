#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>

#include "eventlist.h"
#include "constants.h"

static struct EventList* event_list = NULL;
static unsigned int state_access_delay_ms = 0;
pthread_mutex_t fd_mutex = PTHREAD_MUTEX_INITIALIZER;

/// Calculates a timespec from a delay in milliseconds.
/// @param delay_ms Delay in milliseconds.
/// @return Timespec with the given delay.
static struct timespec delay_to_timespec(unsigned int delay_ms) {
  return (struct timespec){delay_ms / 1000, (delay_ms % 1000) * 1000000};
}

/// Gets the event with the given ID from the state.
/// @note Will wait to simulate a real system accessing a costly memory resource.
/// @param event_id The ID of the event to get.
/// @return Pointer to the event if found, NULL otherwise.
static struct Event* get_event_with_delay(struct ListNode* head, struct ListNode* tail, unsigned int event_id) {
  struct timespec delay = delay_to_timespec(state_access_delay_ms);
  nanosleep(&delay, NULL);  // Should not be removed

  return get_event(head, tail/*event_list*/, event_id);
}

/// Gets the seat with the given index from the state.
/// @note Will wait to simulate a real system accessing a costly memory resource.
/// @param event Event to get the seat from.
/// @param index Index of the seat to get.
/// @return Pointer to the seat.
static unsigned int* get_seat_with_delay(struct Event* event, size_t index) {
  struct timespec delay = delay_to_timespec(state_access_delay_ms);
  nanosleep(&delay, NULL);  // Should not be removed

  return &event->data[index];
}

/// Gets the index of a seat.
/// @note This function assumes that the seat exists.
/// @param event Event to get the seat index from.
/// @param row Row of the seat.
/// @param col Column of the seat.
/// @return Index of the seat.
static size_t seat_index(struct Event* event, size_t row, size_t col) { return (row - 1) * event->cols + col - 1; }

int ems_init(unsigned int delay_ms) {
  if (event_list != NULL) {
    fprintf(stderr, "EMS state has already been initialized\n");
    return 1;
  }

  event_list = create_list();
  state_access_delay_ms = delay_ms;

  return event_list == NULL;
}

int ems_terminate() {
  if (event_list == NULL) {
    fprintf(stderr, "EMS state must be initialized\n");
    return 1;
  }

  pthread_mutex_destroy(&fd_mutex);
  free_list(event_list);
  return 0;
}

int ems_create(unsigned int event_id, size_t num_rows, size_t num_cols) {
  pthread_rwlock_rdlock(&event_list->rwl);
  if (event_list == NULL) {
    fprintf(stderr, "EMS state must be initialized\n");
    pthread_rwlock_unlock(&event_list->rwl);
    return 1;
  }

  if (get_event_with_delay(event_list->head, event_list->tail, event_id) != NULL) {
    fprintf(stderr, "Event already exists\n");
    pthread_rwlock_unlock(&event_list->rwl);
    return 1;
  }
  pthread_rwlock_unlock(&event_list->rwl);

  struct Event* event = malloc(sizeof(struct Event));

  if (event == NULL) {
    fprintf(stderr, "Error allocating memory for event\n");
    return 1;
  }

  event->id = event_id;
  event->rows = num_rows;
  event->cols = num_cols;
  event->reservations = 0;
  event->data = malloc(num_rows * num_cols * sizeof(unsigned int));

  if (event->data == NULL) {
    fprintf(stderr, "Error allocating memory for event data\n");
    free(event);
    return 1;
  }

  for (size_t i = 0; i < num_rows * num_cols; i++) {
    event->data[i] = 0;
  }
  pthread_rwlock_init(&event->rwl, NULL);

  if (append_to_list(event_list, event) != 0) {
    fprintf(stderr, "Error appending event to list\n");
    pthread_rwlock_destroy(&event->rwl);
    free(event->data);
    free(event);
    return 1;
  }

  return 0;
}

int ems_reserve(unsigned int event_id, size_t num_seats, size_t* xs, size_t* ys) {
  pthread_rwlock_rdlock(&event_list->rwl);
  if (event_list == NULL) {
    fprintf(stderr, "EMS state must be initialized\n");
    return 1;
  }
  struct ListNode* head = event_list->head;
  struct ListNode* tail = event_list->tail;
  pthread_rwlock_unlock(&event_list->rwl);

  struct Event* event = get_event_with_delay(head, tail, event_id);

  if (event == NULL) {
    fprintf(stderr, "Event not found\n");
    return 1;
  }
  pthread_rwlock_wrlock(&event->rwl);
  unsigned int reservation_id = ++event->reservations;

  size_t i = 0;
  for (; i < num_seats; i++) {
    size_t row = xs[i];
    size_t col = ys[i];

    if (row <= 0 || row > event->rows || col <= 0 || col > event->cols) {
      fprintf(stderr, "Invalid seat\n");
      break;
    }

    if (*get_seat_with_delay(event, seat_index(event, row, col)) != 0) {
      fprintf(stderr, "Seat already reserved\n");
      break;
    }

    *get_seat_with_delay(event, seat_index(event, row, col)) = reservation_id;
  }

  // If the reservation was not successful, free the seats that were reserved.
  if (i < num_seats) {
    event->reservations--;
    for (size_t j = 0; j < i; j++) {
      *get_seat_with_delay(event, seat_index(event, xs[j], ys[j])) = 0;
    }
    return 1;
  }
  pthread_rwlock_unlock(&event->rwl);

  return 0;
}

int ems_show(unsigned int event_id, int fd) {
  pthread_rwlock_rdlock(&event_list->rwl);
  if (event_list == NULL) {
    fprintf(stderr, "EMS state must be initialized\n");
    pthread_rwlock_unlock(&event_list->rwl);
    return 1;
  }
  // list snapshot
  struct ListNode* head = event_list->head;
  struct ListNode* tail = event_list->tail;
  pthread_rwlock_unlock(&event_list->rwl);

  struct Event* event = get_event_with_delay(head, tail, event_id);
  
  if (event == NULL) {
    fprintf(stderr, "Event not found\n");
    return 1;
  }
  pthread_rwlock_rdlock(&event->rwl);
  // buffer should have enough space for seats, newlines, spaces and the null terminator:
  // unsimplified expression:
  // UINT_MAX_DIGITS*event->rows*event->cols + event->rows + (event->cols - 1)*event->rows + 1
  char buffer[event->rows*event->cols*(UINT_MAX_DIGITS + 1) + 1];
  size_t rows = event->rows;
  size_t cols = event->cols;
  pthread_rwlock_unlock(&event->rwl);
  size_t buffer_pos = 0;

  for (size_t i = 1; i <= rows; i++) {
    for (size_t j = 1; j <= cols; j++) {
      pthread_rwlock_rdlock(&event->rwl);
      unsigned int* seat = get_seat_with_delay(event, seat_index(event, i, j));
      pthread_rwlock_unlock(&event->rwl);
      size_t len = (size_t)snprintf(buffer + buffer_pos, sizeof(buffer) - buffer_pos, "%u", *seat);

      if (len >= sizeof(buffer) - buffer_pos) {
        perror("Error formatting to buffer");        
        return 1;
      }

      buffer_pos += len;

      if (j < cols) {
        len = (size_t)snprintf(buffer + buffer_pos, sizeof(buffer) - buffer_pos, " ");
        if (len >= sizeof(buffer) - buffer_pos) {
            perror("Error formatting to buffer");
            return 1;
        }
        buffer_pos += len;
      }
    }
    size_t len = (size_t)snprintf(buffer + buffer_pos, sizeof(buffer) - buffer_pos, "\n");

    if (len >= sizeof(buffer) - buffer_pos) {
      perror("Error formatting to buffer");
      return 1;
    }
    buffer_pos += len;
  }
  // Write the entire buffer to fd
  pthread_mutex_lock(&fd_mutex);
  if (write(fd, buffer, buffer_pos) < 0) {
    perror("Error writing to file");
    pthread_mutex_unlock(&fd_mutex);
    return 1;
  }
  pthread_mutex_unlock(&fd_mutex);
  return 0;
}

int ems_list_events(int fd) {
  pthread_rwlock_rdlock(&event_list->rwl);
  if (event_list == NULL) {
    fprintf(stderr, "EMS state must be initialized\n");
    pthread_rwlock_unlock(&event_list->rwl);
    return 1;
  }

  if (event_list->head == NULL) {
    pthread_rwlock_unlock(&event_list->rwl);
    pthread_mutex_lock(&fd_mutex);
    if (write(fd, "No events\n", 11) < 0) {
      pthread_mutex_unlock(&fd_mutex);
      perror("Error writing to file");
      return 1;
    }
    pthread_mutex_unlock(&fd_mutex);
    return 0;
  }
  pthread_rwlock_unlock(&event_list->rwl);

  size_t buffer_size = 9 + UINT_MAX_DIGITS;
  char *buffer = (char*)malloc(sizeof(char)*(buffer_size));

  if (buffer == NULL) {
    perror("Error allocating memory");
    return 1;
  }

  size_t buffer_pos = 0;
  pthread_rwlock_rdlock(&event_list->rwl);
  struct ListNode* head = event_list->head;
  struct ListNode* tail = event_list->tail;
  pthread_rwlock_unlock(&event_list->rwl);

  struct ListNode* current = head;
  while (current != tail->next) {
    
    if ((buffer_pos + 9 + UINT_MAX_DIGITS) > buffer_size) {
        // Expand the buffer
        buffer_size *= 2;
        buffer = (char *)realloc(buffer, buffer_size);
        if (buffer == NULL) {
            perror("Error reallocating memory");
            return 1;
        }
    }
    pthread_rwlock_rdlock(&(current->event)->rwl);
    size_t len = (size_t)snprintf(buffer + buffer_pos, buffer_size - buffer_pos, "Event: %u\n", (current->event)->id);
    pthread_rwlock_unlock(&(current->event)->rwl);
    buffer_pos += len;
    current = current->next;
  }
  pthread_mutex_lock(&fd_mutex);
  if (write(fd, buffer, buffer_pos) < 0) {
    pthread_mutex_unlock(&fd_mutex);
    perror("Error writing to file");
    free(buffer);
    return 1;
  }
  pthread_mutex_unlock(&fd_mutex);
  free(buffer);
  return 0;
}

void ems_wait(unsigned int delay_ms) {
  struct timespec delay = delay_to_timespec(delay_ms);
  nanosleep(&delay, NULL);
}
