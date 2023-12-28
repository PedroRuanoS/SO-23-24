#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>

#include "api.h"
#include "common/io.h"

static int req_pipe;
static int resp_pipe;
static int reg_client;

static int session_id;

void fill_str(char result_array[], const char *str) {
  strcpy(result_array, str);
  
  // Fill the remaining positions with '\0' characters
  size_t len = strlen(str);
  for (size_t i = len; i < sizeof(result_array); ++i) {
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

  const char OP_CODE = '1'; // letra maiuscula?
  char req_path[40];
  char resp_path[40];
  char req_message[82];

  fill_str(req_path, req_pipe_path);
  fill_str(resp_path, resp_pipe_path);
  snprintf(req_message, 82, "%c%s%s", OP_CODE, req_path, resp_path);
  
  print_str(reg_client, req_message); // sends the register request to the server
  
  // Open requests pipe for writing
  // This waits for the server to open it for reading
  req_pipe = open(req_pipe_path, O_WRONLY);
  if (req_pipe == -1) {
    fprintf(stderr, "open failed: %s\n", strerror(errno));
    return 1;
  }

  // Open responses pipe for reading
  // This waits for the server to open it for writing
  resp_pipe = open(resp_pipe_path, O_RDONLY);
  if (req_pipe == -1) {
    fprintf(stderr, "open failed: %s\n", strerror(errno));
    return 1;
  }

  // Obtain the response from the server with the session id
  char buffer[sizeof(int)];
  ssize_t ret = read(resp_pipe, buffer, sizeof(int));

  if (ret != sizeof(int)) { // ou while até receber o inteiro?
    fprintf(stderr, "read failed: %s\n", strerror(errno));
    return 1;
  }

  memcpy(&session_id, buffer, sizeof(int)); // store the session id

  return 0;
}

int ems_quit(void) {
  //TODO: close pipes

  // tell the server to end this session
  if (write(req_pipe, "2", 1) == -1) {
    fprintf(stderr, "write failed: %s\n", strerror(errno));
    return 1;
  }

  close(reg_client);
  close(req_pipe);
  close(resp_pipe);
  return 0;
}

int ems_create(unsigned int event_id, size_t num_rows, size_t num_cols) {
  //TODO: send create request to the server (through the request pipe) and wait for the response (through the response pipe)
  return 1;
}

int ems_reserve(unsigned int event_id, size_t num_seats, size_t* xs, size_t* ys) {
  //TODO: send reserve request to the server (through the request pipe) and wait for the response (through the response pipe)
  return 1;
}

int ems_show(int out_fd, unsigned int event_id) {
  //TODO: send show request to the server (through the request pipe) and wait for the response (through the response pipe)
  return 1;
}

int ems_list_events(int out_fd) {
  //TODO: send list request to the server (through the request pipe) and wait for the response (through the response pipe)
  return 1;
}
