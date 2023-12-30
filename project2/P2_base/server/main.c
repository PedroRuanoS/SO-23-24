#include <limits.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "common/constants.h"
#include "common/io.h"
#include "operations.h"
#include "eventlist.h"

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
      if (new_client.resp_pipe == -1) {
        fprintf(stderr, "open failed: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
      }

      // Respond to the client with the corresponding session id
      char buff[4];
      sprintf(buff, "%d", new_client.session_id);
      if (print_str(new_client.resp_pipe, buff)) {
        fprintf(stderr, "Error writing to pipe: %s\n", strerror(errno));
        exit(EXIT_FAILURE); // ver pergunta 100 no piazza
      }
    }

    //TODO: Write new client to the producer-consumer buffer
  //}
  int quit = 0;
  while (1) {
    char op_buffer[sizeof(char) + sizeof(int) + sizeof(unsigned int) + sizeof(size_t) + 
      2 * MAX_RESERVATION_SIZE * sizeof(size_t) + 1];
    ssize_t op_bytes_read = read(new_client.req_pipe, op_buffer, sizeof(buffer)); 
    if (op_bytes_read == 0) {
      fprintf(stderr, "Responses pipe closed\n");
      exit(EXIT_FAILURE);
    } else if (op_bytes_read == -1) {
      fprintf(stderr, "Read failed: %s\n", strerror(errno));
      exit(EXIT_FAILURE);
    } else {
      int op_code = op_buffer[0];
      unsigned int event_id;
      unsigned int *seats = NULL, *ids = NULL;
      size_t num_rows, num_cols, num_seats, num_events;
      size_t xs[MAX_RESERVATION_SIZE], ys[MAX_RESERVATION_SIZE];
      int offset = 0, response = 0;
      switch (op_code) {
        case '2':
          //TODO: clear session_id
          quit = 1; 
          break;

        case '3':
          //use pos??
          memcpy(&event_id, &op_buffer[2], sizeof(event_id));
          memcpy(&num_rows, &op_buffer[6], sizeof(num_rows));
          memcpy(&num_cols, &op_buffer[14], sizeof(num_cols));

          if (print_uint(new_client.resp_pipe, (unsigned int) ems_create(event_id, num_rows, num_cols))) {
            fprintf(stderr, "Error writing to responses pipe: %s\n", strerror(errno));
            return 1;
          }  
          break;

        case '4':
          //use pos??
          memcpy(&event_id, &op_buffer[2], sizeof(event_id));
          memcpy(&num_seats, &op_buffer[6], sizeof(num_seats));
          memcpy(&xs, &op_buffer[14], num_seats * sizeof(size_t));
          memcpy(&ys, &op_buffer[14 + num_seats * sizeof(size_t)], num_seats * sizeof(size_t));

          if (print_uint(new_client.resp_pipe, (unsigned int) ems_reserve(event_id, num_seats, xs, ys))) {
            fprintf(stderr, "Error writing to responses pipe: %s\n", strerror(errno));
            return 1;
          }       
          break;

        case '5':
          //use pos??
          memcpy(&event_id, &op_buffer[2], sizeof(event_id));
          response = ems_show(event_id, &num_rows, &num_cols, seats);
          size_t show_message_size = sizeof(int) + sizeof(size_t) + sizeof(size_t) + 
            sizeof(unsigned int) * num_rows * num_cols + 1;
          char *show_message = malloc(show_message_size);
          if (show_message == NULL) {
            perror("Memory allocation failed");
            return 1;
          }

          if (response) {
            snprintf(show_message, show_message_size, "%d", response);
          } else {
            offset = snprintf(show_message, show_message_size, "%d%zu%zu", 
                            response, num_rows, num_cols);
            for (size_t i = 0; i < num_rows * num_cols; i++) {
              offset += snprintf(show_message + offset, show_message_size - (size_t) offset, "%u", seats[i]);
            }
            free(seats);
          }
          
          if (print_str(new_client.resp_pipe, show_message)) {
            fprintf(stderr, "Error writing to responses pipe: %s\n", strerror(errno));
            return 1;
          }
          break;

        case '6':
          response = ems_list_events(&num_events, ids);
          size_t list_message_size = sizeof(int) + sizeof(size_t) + sizeof(unsigned int)* num_events;
          char *list_message = malloc(list_message_size);
          if (list_message == NULL) {
            perror("Memory allocation failed");
            return 1;
          }

          if (response) {
            snprintf(show_message, show_message_size, "%d", response);
          } else {
            offset = snprintf(list_message, list_message_size, "%d%zu", response, num_events);
            for (size_t i = 0; i < num_rows * num_cols; i++) {
              offset += snprintf(list_message + offset, list_message_size - (size_t) offset, "%u", ids[i]);
            }
            free(ids);
          }

          if (print_str(new_client.resp_pipe, list_message)) {
            fprintf(stderr, "Error writing to responses pipe: %s\n", strerror(errno));
            return 1;
          }
          break;
      }
    }
    if (quit)
      break;
  }
  //TODO: Close Server
  close(reg_server);

  ems_terminate();
}