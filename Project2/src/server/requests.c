#include "requests.h"

int read_session_id(int req_pipe, int *session_id) {
  ssize_t bytes_read = read(req_pipe, session_id, sizeof(int));

  if (bytes_read == 0) {
    fprintf(stderr, "Requests pipe closed\n");
    return 1;
  } else if (bytes_read == -1) {
    fprintf(stderr, "Error reading from requests pipe: %s\n", strerror(errno));
    return 1;
  }

  return 0;
}

int read_event_id(int req_pipe, unsigned int *event_id) {
  ssize_t bytes_read = read(req_pipe, event_id, sizeof(unsigned int));

  if (bytes_read == 0) {
    fprintf(stderr, "Requests pipe closed\n");
    return 1;
  } else if (bytes_read == -1) {
    fprintf(stderr, "Error reading from requests pipe: %s\n", strerror(errno));
    return 1;
  }

  return 0;
}

int read_create_request(int req_pipe, int *session_id, unsigned int *event_id, size_t *num_rows, size_t *num_cols) {
  size_t num_rc[2];

  if (read_session_id(req_pipe, session_id) || read_event_id(req_pipe, event_id))
    return 1;

  ssize_t bytes_read = read(req_pipe, num_rc, sizeof(num_rc));

  if (bytes_read == 0) {
    fprintf(stderr, "Requests pipe closed\n");
    return 1;
  } else if (bytes_read == -1) {
    fprintf(stderr, "Error reading from requests pipe: %s\n", strerror(errno));
    return 1;
  }

  *num_rows = num_rc[0];
  *num_cols = num_rc[1];

  return 0;
}

int read_reserve_request(int req_pipe, int *session_id, unsigned int *event_id, size_t *num_seats, size_t *xs, size_t *ys) {
  
  if (read_session_id(req_pipe, session_id) || read_event_id(req_pipe, event_id))
    return 1;

  ssize_t bytes_read = read(req_pipe, num_seats, sizeof(size_t));

  if (bytes_read == 0) {
    fprintf(stderr, "Requests pipe closed\n");
    return 1;
  } else if (bytes_read == -1) {
    fprintf(stderr, "Error reading from requests pipe: %s\n", strerror(errno));
    return 1;
  }

  size_t xys[(*num_seats)*2];

  bytes_read = read(req_pipe, xys, sizeof(xys));

  if (bytes_read == 0) {
    fprintf(stderr, "Requests pipe closed\n");
    return 1;
  } else if (bytes_read == -1) {
    fprintf(stderr, "Error reading from requests pipe: %s\n", strerror(errno));
    return 1;
  }

  for (size_t i = 0; i < *num_seats; i++) {
    xs[i] = xys[i];
    ys[i] = xys[i + *num_seats];
  }

  return 0;
}

int read_show_request(int req_pipe, int *session_id, unsigned int *event_id) {
  if (read_session_id(req_pipe, session_id) || read_event_id(req_pipe, event_id))
    return 1;

  return 0;
}