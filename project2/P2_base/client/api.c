#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>

#include "api.h"
#include "common/io.h"
#include "common/constants.h"

static int req_pipe;
static int resp_pipe;
static int reg_client;

static int session_id;

void fill_str(char result_array[], size_t size, const char *str) {
  strcpy(result_array, str);
  
  // Fill the remaining positions with '\0' characters
  size_t len = strlen(str);
  for (size_t i = len; i < size; ++i) {
    result_array[i] = '\0';
  }
}

int ems_setup(char const* req_pipe_path, char const* resp_pipe_path, char const* server_pipe_path) {
  //TODO: create pipes and connect to the server
  if (unlink(req_pipe_path) != 0 && errno != ENOENT) {
    fprintf(stderr, "unlink(%s) failed: %s\n", req_pipe_path,
            strerror(errno));
    return 1;
  }

  if (unlink(resp_pipe_path) != 0 && errno != ENOENT) {
    fprintf(stderr, "unlink(%s) failed: %s\n", resp_pipe_path,
            strerror(errno));
    return 1;
  }

  // Create requests pipe
  if (mkfifo(req_pipe_path, 0640) != 0) {
    fprintf(stderr, "mkfifo failed: %s\n", strerror(errno));
    return 1;
  }

  // Create responses pipe
  if (mkfifo(resp_pipe_path, 0640) != 0) {
    fprintf(stderr, "mkfifo failed: %s\n", strerror(errno));
    return 1;
  }

  // Open the register pipe for writing
  // This waits for the server to open it for reading
  reg_client = open(server_pipe_path, O_WRONLY);
  if (reg_client == -1) {
    fprintf(stderr, "open failed: %s\n", strerror(errno));
    return 1;
  }

  printf("Client opened register pipe\n");

  const char OP_CODE = '1'; // letra maiuscula?
  char req_path[40];
  char resp_path[40];

  fill_str(req_path, sizeof(req_path), req_pipe_path);
  fill_str(resp_path, sizeof(resp_path), resp_pipe_path);
  
  // Send the register request to the server
  if (write_str(reg_client, &OP_CODE, 1) || write_str(reg_client, req_path, 40*sizeof(char)) || write_str(reg_client, resp_path, 40*sizeof(char))) {
    fprintf(stderr, "Error writing to register pipe: %s\n", strerror(errno));
    return 1;
  } 

  printf("Client stuck opening request pipe\n");
  
  // Open requests pipe for writing
  // This waits for the server to open it for reading
  req_pipe = open(req_pipe_path, O_WRONLY);
  
  if (req_pipe == -1) {
    fprintf(stderr, "open failed: %s\n", strerror(errno));
    return 1;
  }

  printf("Client opened request pipe\n");

  // Open responses pipe for reading
  // This waits for the server to open it for writing
  resp_pipe = open(resp_pipe_path, O_RDONLY);
  if (resp_pipe == -1) {
    fprintf(stderr, "open failed: %s\n", strerror(errno));
    return 1;
  }

  printf("Client opened responses pipe\n");

  // Obtain the response from the server with the session id
  char buffer[sizeof(int) + 1]; // é preciso o +1 neste caso?
  ssize_t ret = read(resp_pipe, buffer, sizeof(int));

  // sizeof(int) ou sizeof(buffer)?
  if (ret == 0) {
    fprintf(stderr, "responses pipe closed\n");
    return 1;
  } else if (ret == -1) { // ou while até receber o inteiro?
    fprintf(stderr, "Error reading from responses pipe: %s\n", strerror(errno));
    return 1;
  }

  memcpy(&session_id, &buffer[0], sizeof(int)); // store the session id

  printf("Session_id stored: %d\n", session_id);

  return 0;
}

int ems_quit(void) {
  //TODO: close pipes

  const char OP_CODE = '2';

  // Send a request to the server to end this session
  if (write_str(req_pipe, &OP_CODE, 1) || write_int(req_pipe, session_id)) {
    fprintf(stderr, "Error writing to requests pipe: %s\n", strerror(errno));
    return 1;
  }

  close(reg_client);
  close(req_pipe);
  close(resp_pipe);
  return 0;
}

int ems_create(unsigned int event_id, size_t num_rows, size_t num_cols) {
  //TODO: send create request to the server (through the request pipe) and wait for the response (through the response pipe)

  const char OP_CODE = '3';
  size_t num_rc[2];
  num_rc[0] = num_rows;
  num_rc[1] = num_cols;
  //char req_message[sizeof(int) + sizeof(unsigned int) + sizeof(size_t)*2 + 2];
  //snprintf(req_message, sizeof(req_message), "%c%d%u%zu%zu", OP_CODE, session_id, event_id, num_rows, num_cols);

  // Send a create request to the server

  printf("ems_create | session_id: %d event_id: %u num_rows: %zu num_cols: %zu\n", session_id, event_id, num_rc[0], num_rc[1]);
  if (write_str(req_pipe, &OP_CODE, 1) || write_int(req_pipe, session_id) || 
      write_uint(req_pipe, event_id) || write_sizet_array(req_pipe, num_rc, 2)) {
    fprintf(stderr, "Error writing to requests pipe: %s\n", strerror(errno));
    return 1;
  }

  // Obtain the response from the server
  int response;
  // char resp_message[sizeof(int) + 1];
  ssize_t ret = read(resp_pipe, &response, sizeof(int));

  // sizeof(int) ou sizeof(buffer)?
  if (ret == 0) {
    fprintf(stderr, "responses pipe closed\n");
    return 1;
  } else if (ret == -1) { // ou while até receber o inteiro?
    fprintf(stderr, "Error reading from responses pipe: %s\n", strerror(errno));
    return 1;
  }

  if (response) {
    fprintf(stderr, "Create failed\n");
    return 1;
  }

  return 0;
}

int ems_reserve(unsigned int event_id, size_t num_seats, size_t* xs, size_t* ys) {
  //TODO: send reserve request to the server (through the request pipe) and wait for the response (through the response pipe)

  const char OP_CODE = '4';
  /*
  char req_message[sizeof(int) + sizeof(unsigned int) + sizeof(size_t) + sizeof(xs) + sizeof(ys) + 2];
  int bytes_written = snprintf(req_message, sizeof(req_message), "%c%d%u%zu", OP_CODE, session_id, event_id, num_seats);

  for (size_t i = 0; i < num_seats; i++) {
    bytes_written += snprintf(req_message + bytes_written, sizeof(req_message) - (size_t)bytes_written, "%zu", xs[i]);
  }

  for (size_t i = 0; i < num_seats; i++) {
    bytes_written += snprintf(req_message + bytes_written, sizeof(req_message) - (size_t)bytes_written, "%zu", ys[i]);
  }
  */

  size_t xys[num_seats*2];
  for (size_t i = 0; i < num_seats; i++) {
    xys[i] = xs[i];
    xys[i + num_seats] = ys[i];
  }

  // Send a reserve request to the server
  if (write_str(req_pipe, &OP_CODE, 1) || write_int(req_pipe, session_id) || write_uint(req_pipe, event_id)
      || write_sizet(req_pipe, num_seats) || write_sizet_array(req_pipe, xys, num_seats*2)) {
    fprintf(stderr, "Error writing to requests pipe: %s\n", strerror(errno));
    return 1;
  }
  
  // Obtain the response from the server
  int response;
  ssize_t ret = read(resp_pipe, &response, sizeof(int));

  if (ret == 0) {
    fprintf(stderr, "responses pipe closed\n");
    return 1;
  } else if (ret == -1) { // ou while até receber o inteiro?
    fprintf(stderr, "Error reading from responses pipe: %s\n", strerror(errno));
    return 1;
  }

  if (response) {
    fprintf(stderr, "Reserve failed\n");
    return 1;
  }
  
  return 0;
}

int ems_show(int out_fd, unsigned int event_id) {
  //TODO: send show request to the server (through the request pipe) and wait for the response (through the response pipe)
  
  const char OP_CODE = '5';

  if (write_str(req_pipe, &OP_CODE, 1) || write_int(req_pipe, session_id) || write_uint(req_pipe, event_id)) {
    fprintf(stderr, "Error writing to requests pipe: %s\n", strerror(errno));
    return 1;
  }

  int response;
  ssize_t bytes_read = read(resp_pipe, &response, sizeof(int));
  
  if (bytes_read == 0) {
    fprintf(stderr, "responses pipe closed\n");
    return 1;
  } else if (bytes_read == -1) {
    fprintf(stderr, "Error reading from responses pipe: %s\n", strerror(errno));
    return 1;
  }

  if (response) {
    fprintf(stderr, "Show failed\n");
    return 1;
  }

  size_t num_rows;
  size_t num_cols;
  bytes_read = read(resp_pipe, &num_rows, sizeof(size_t));

  if (bytes_read == 0) {
    fprintf(stderr, "responses pipe closed\n");
    return 1;
  } else if (bytes_read == -1) {
    fprintf(stderr, "Error reading from responses pipe: %s\n", strerror(errno));
    return 1;
  }

  bytes_read = read(resp_pipe, &num_cols, sizeof(size_t));

  if (bytes_read == 0) {
    fprintf(stderr, "responses pipe closed\n");
    return 1;
  } else if (bytes_read == -1) {
    fprintf(stderr, "Error reading from responses pipe: %s\n", strerror(errno));
    return 1;
  }

  unsigned int seats[sizeof(unsigned int)*num_rows*num_cols];
  bytes_read = read(resp_pipe, seats, sizeof(seats));
  if (bytes_read == 0) {
    fprintf(stderr, "responses pipe closed\n");
    return 1;
  } else if (bytes_read == -1) {
    fprintf(stderr, "Error reading from responses pipe: %s\n", strerror(errno));
    return 1;
  }

  for (size_t i = 0; i < num_rows; i++) {
    for (size_t j = 0; j < num_cols; j++) {
      char buffer[16];
      sprintf(buffer, "%u", seats[i*num_cols + j]);

      printf("seats[%zu] = %u\n", i*num_cols + j, seats[i*num_cols + j]);

      if (print_str(out_fd, buffer)) {
        perror("Error writing to file descriptor");
        return 1;
      }

      if (j < num_cols - 1) {
        if (print_str(out_fd, " ")) {
        perror("Error writing to file descriptor");
        return 1;
        }
      }
    }

    if (print_str(out_fd, "\n")) {
      perror("Error writing to file descriptor");
      return 1;
    }
  }

  return 0;
}

int ems_list_events(int out_fd) {
  //TODO: send list request to the server (through the request pipe) and wait for the response (through the response pipe)
  
  const char OP_CODE = '6';
  // char req_message[sizeof(int) + 2];
  // snprintf(req_message, sizeof(req_message), "%c%d", OP_CODE, session_id);

  // Send a list request to the server
  if (write_str(req_pipe, &OP_CODE, 1) || write_int(req_pipe, session_id)) {
    fprintf(stderr, "Error writing to requests pipe: %s\n", strerror(errno));
    return 1;
  }

  int response;
  ssize_t bytes_read = read(resp_pipe, &response, sizeof(int));
  
  if (bytes_read == 0) {
    fprintf(stderr, "responses pipe closed\n");
    return 1;
  } else if (bytes_read == -1) {
    fprintf(stderr, "Error reading from responses pipe: %s\n", strerror(errno));
    return 1;
  }

  if (response) {
    fprintf(stderr, "List events failed\n");
    return 1;
  }

  size_t num_events;
  bytes_read = read(resp_pipe, &num_events, sizeof(ssize_t));

  if (bytes_read == 0) {
    fprintf(stderr, "responses pipe closed\n");
    return 1;
  } else if (bytes_read == -1) {
    fprintf(stderr, "Error reading from responses pipe: %s\n", strerror(errno));
    return 1;
  }

  if (num_events == 0) {
    char buff[] = "No events\n";
    if (print_str(out_fd, buff)) {
      perror("Error writing to file descriptor");
      return 1;
    }
    return 0;
  }

  unsigned int ids[sizeof(unsigned int)*num_events];
  bytes_read = read(resp_pipe, ids, sizeof(ids));

  if (bytes_read == 0) {
    fprintf(stderr, "responses pipe closed\n");
    return 1;
  } else if (bytes_read == -1) {
    fprintf(stderr, "Error reading from responses pipe: %s\n", strerror(errno));
    return 1;
  }

  for (size_t i = 0; i < num_events; i++) {
    char buff[] = "Event: ";

    if (print_str(out_fd, buff)) {
      perror("Error writing to file descriptor");
      return 1;
    }

    char id[16];
    sprintf(id, "%u\n", ids[i]);
    if (print_str(out_fd, id)) {
      perror("Error writing to file descriptor");
      return 1;
    }
  }

  return 0;
}
