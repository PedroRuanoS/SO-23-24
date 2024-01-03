#include "requests.h"

void read_session_id(int req_pipe, int *session_id) {
  printf("read_session_id: %d req_pipe fd: %d)\n", *session_id, req_pipe);
  ssize_t bytes_read = read(req_pipe, session_id, sizeof(int));

  printf("read_session_id: %d bytes_read: %zd\n", *session_id, bytes_read);

  if (bytes_read == 0) {
    fprintf(stderr, "requests pipe closed\n");
    exit(EXIT_FAILURE); // alterar quando adicionarmos threads
  } else if (bytes_read == -1) {
    fprintf(stderr, "Error reading from requests pipe: %s\n", strerror(errno));
    exit(EXIT_FAILURE); // alterar quando adicionarmos threads
  }
}

void read_event_id(int req_pipe, unsigned int *event_id) {
  ssize_t bytes_read = read(req_pipe, event_id, sizeof(unsigned int));

  printf("read_event_id: %u bytes_read: %zd\n", *event_id, bytes_read);

  if (bytes_read == 0) {
    fprintf(stderr, "requests pipe closed\n");
    exit(EXIT_FAILURE); // alterar quando adicionarmos threads
  } else if (bytes_read == -1) {
    fprintf(stderr, "Error reading from requests pipe: %s\n", strerror(errno));
    exit(EXIT_FAILURE); // alterar quando adicionarmos threads
  }
}

void read_create_request(int req_pipe, int *session_id, unsigned int *event_id, size_t *num_rows, size_t *num_cols) {
  size_t num_rc[2]; // alterar nome

  read_session_id(req_pipe, session_id);
  read_event_id(req_pipe, event_id);

  //////
  printf("read_create_request | session_id: %d event_id: %u\n", *session_id, *event_id);

  ssize_t bytes_read = read(req_pipe, num_rc, sizeof(num_rc));

  if (bytes_read == 0) {
    fprintf(stderr, "requests pipe closed\n");
    exit(EXIT_FAILURE); // alterar quando adicionarmos threads
  } else if (bytes_read == -1) {
    fprintf(stderr, "Error reading from requests pipe: %s\n", strerror(errno));
    exit(EXIT_FAILURE); // alterar quando adicionarmos threads
  }

  printf("read_create_request | bytes_read: %zd num_rows: %zu num_cols: %zu\n", bytes_read, *num_rows, *num_cols);

  *num_rows = num_rc[0];
  *num_cols = num_rc[1];

  printf("read_create_request | num_rows: %zu num_cols: %zu\n", *num_rows, *num_cols);
}

void read_reserve_request(int req_pipe, int *session_id, unsigned int *event_id, size_t *num_seats, size_t *xs, size_t *ys) {
  size_t xys[MAX_RESERVATION_SIZE*2];

  read_session_id(req_pipe, session_id);
  read_event_id(req_pipe, event_id);

  ssize_t bytes_read = read(req_pipe, num_seats, sizeof(size_t));

  if (bytes_read == 0) {
    fprintf(stderr, "requests pipe closed\n");
    exit(EXIT_FAILURE); // alterar quando adicionarmos threads
  } else if (bytes_read == -1) {
    fprintf(stderr, "Error reading from requests pipe: %s\n", strerror(errno));
    exit(EXIT_FAILURE); // alterar quando adicionarmos threads
  }

  bytes_read = read(req_pipe, xys, sizeof(xys));

  if (bytes_read == 0) {
    fprintf(stderr, "requests pipe closed\n");
    exit(EXIT_FAILURE); // alterar quando adicionarmos threads
  } else if (bytes_read == -1) {
    fprintf(stderr, "Error reading from requests pipe: %s\n", strerror(errno));
    exit(EXIT_FAILURE); // alterar quando adicionarmos threads
  }

  for (size_t i = 0; i < *num_seats; i++) {
    xs[i] = xys[i];
    ys[i] = xys[i + *num_seats];
  }
}

void read_show_request(int req_pipe, int *session_id, unsigned int *event_id) {
  read_session_id(req_pipe, session_id);
  read_event_id(req_pipe, event_id);
}