#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

#include "common/constants.h"
#include "common/io.h"
#include "operations.h"

typedef struct {
  int session_id;
  int req_pipe;
  int resp_pipe;
} Client;

int main(int argc, char* argv[]) {
  if (argc < 2 || argc > 3) {
    fprintf(stderr, "Usage: %s\n <pipe_path> [delay]\n", argv[0]);
    return 1;
  }

  char* endptr;
  unsigned int state_access_delay_us = STATE_ACCESS_DELAY_US;
  if (argc == 3) {
    unsigned long int delay = strtoul(argv[2], &endptr, 10);

    if (*endptr != '\0' || delay > UINT_MAX) {
      fprintf(stderr, "Invalid delay value or value too large\n");
      return 1;
    }

    state_access_delay_us = (unsigned int)delay;
  }

  if (ems_init(state_access_delay_us)) {
    fprintf(stderr, "Failed to initialize EMS\n");
    return 1;
  }

  //TODO: Initialize server, create worker threads
  char pipe_path[MAX_JOB_FILE_NAME_SIZE]; // alterar constante max job?
  strcpy(pipe_path, argv[1]);

  if (unlink(pipe_path) != 0 && errno != ENOENT) {
    fprintf(stderr, "unlink(%s) failed: %s\n", pipe_path,
            strerror(errno));
    exit(EXIT_FAILURE);
  }

  // Create register pipe
  if (mkfifo(pipe_path, 0640) != 0) { // alterar permissoes?
    fprintf(stderr, "mkfifo failed: %s\n", strerror(errno));
    exit(EXIT_FAILURE);
  }

  // Open register pipe for reading
  // This waits for a client to open it for writing
  int reg_server = open(pipe_path, O_RDONLY);
  if (reg_server == -1) {
    fprintf(stderr, "open failed: %s\n", strerror(errno));
    exit(EXIT_FAILURE);
  }
  
  // por esta linha dentro do while quando tivermos multiplas sessoes
  Client new_client = {.session_id = 0}; // alterar lógica de criação do session id
  
  //while (1) {
    //TODO: Read from pipe
    char buffer[82];
    ssize_t bytes_read = read(reg_server, buffer, 81);
    if (bytes_read == 0) { // diferente neste caso como mudar?
      fprintf(stderr, "register pipe closed\n");
      exit(EXIT_FAILURE);
    } else if (bytes_read == -1) {
      fprintf(stderr, "read failed: %s\n", strerror(errno));
      exit(EXIT_FAILURE);
    } else if (bytes_read == sizeof(buffer)) {
      char op_code = buffer[0];
      char req_pipe_path[40];
      char resp_pipe_path[40];

      if (op_code != '1') {
        fprintf(stderr, "Operation code must be 1 for setup\n");
        exit(EXIT_FAILURE);
      }
      
      for (int i = 0; i < 40; i++) {
        req_pipe_path[i] = buffer[i+1];
        resp_pipe_path[i] = buffer[i+41];
      }

      // Open requests pipe for reading
      // This waits for the client to open it for writing
      new_client.req_pipe = open(req_pipe_path, O_RDONLY);
      if (new_client.req_pipe == -1) {
        fprintf(stderr, "open failed: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
      }

      // Open responses pipe for writing
      // This waits for the client to open it for reading
      new_client.resp_pipe = open(resp_pipe_path, O_WRONLY);
      if (new_client.req_pipe == -1) {
        fprintf(stderr, "open failed: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
      }

      // Respond to the client with the corresponding session id
      if (print_str(new_client.resp_pipe, (char*)new_client.session_id)) {
        fprintf(stderr, "Error writing to pipe: %s\n", strerror(errno));
        exit(EXIT_FAILURE); // ver pergunta 100 no piazza
      }
    }

    //TODO: Write new client to the producer-consumer buffer
  //}

  while (1) {
    
  }

  //TODO: Close Server
  close(reg_server);

  ems_terminate();
}