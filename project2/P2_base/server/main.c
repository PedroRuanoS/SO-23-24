#include <limits.h>
#include <pthread.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <pthread.h>
#include <semaphore.h>

#include "common/constants.h"
#include "common/io.h"
#include "operations.h"
#include "requests.h"
#include "prod_cons_queue.h"

pthread_mutex_t sessions_mutex = PTHREAD_MUTEX_INITIALIZER;
sem_t semSessions;
int sessions[MAX_SESSION_COUNT] = {0}; // apagar da main

void *consumer_thread_fn(void* arg) {
  ClientQueue *queue = (ClientQueue*) arg;

  while (1) {
    Client new_client = dequeue(queue);
    int quit = 0;
    int session_id; // alterar session id variaveis duplicadas
    int req_pipe;
    int resp_pipe;

    // TODO: generate session_id
    //pthread_cond_wait(&cond);
    sem_wait(&semSessions);
    for (int i = 0; i < MAX_SESSION_COUNT; i++) {
      
      if (pthread_mutex_lock(&sessions_mutex) != 0) {
        fprintf(stderr, "Error locking sessions mutex\n");
        //return 1;
      }

      if (sessions[i] == 0) {
        session_id = i;
        sessions[i] = 1;

        if (pthread_mutex_unlock(&sessions_mutex) != 0) {
          fprintf(stderr, "Error unlocking sessions mutex\n");
          //return 1;
        }
        break;
      }
      if (pthread_mutex_unlock(&sessions_mutex) != 0) {
        fprintf(stderr, "Error unlocking sessions mutex\n");
        //return 1;
      }
    }

    req_pipe = open(new_client.req_pipe_path, O_RDONLY);
    if (req_pipe == -1) {
      fprintf(stderr, "open failed: %s\n", strerror(errno));
      exit(EXIT_FAILURE);
    }

    printf("Server opened request pipe, fd: %d\n", req_pipe);

    // Open responses pipe for writing
    // This waits for the client to open it for reading
    resp_pipe = open(new_client.resp_pipe_path, O_WRONLY);
    if (resp_pipe == -1) {
      fprintf(stderr, "open failed: %s\n", strerror(errno));
      exit(EXIT_FAILURE);
    }

    printf("Server opened responses pipe\n");

    // TODO: generate session_id
    for (int i = 0; i < MAX_SESSION_COUNT; i++) {
      pthread_mutex_lock(&sessions_mutex);
      if (sessions[i] == 0) {
        session_id = i;
        sessions[i] = 1;
        pthread_mutex_unlock(&sessions_mutex);
        break;
      }
      pthread_mutex_unlock(&sessions_mutex);
    }

    // Respond to the client with the corresponding session id
    if (write_int(resp_pipe, session_id)) {
      fprintf(stderr, "Error writing to pipe: %s\n", strerror(errno));
      exit(EXIT_FAILURE); // ver pergunta 100 no piazza
    }

    while (1) {
      char op_buffer;
      ssize_t op_bytes_read = read(req_pipe, &op_buffer, sizeof(char));
      if (op_bytes_read == 0) {
        //TODO: clear session_id
        if (pthread_mutex_lock(&sessions_mutex) != 0) {
          fprintf(stderr, "Error locking sessions mutex\n");
          //return 1;
        }

        sessions[session_id] = 0;
        sem_post(&semSessions);
        if (pthread_mutex_unlock(&sessions_mutex) != 0) {
          fprintf(stderr, "Error unlocking sessions mutex\n");
          //return 1;
        }
        quit = 1;        
      } else if (op_bytes_read == -1) {
        fprintf(stderr, "Read failed: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
      } else {
        int resp_session_id = 0;
        unsigned int event_id;
        unsigned int *seats = NULL, *ids = NULL;
        size_t num_rows, num_cols, num_seats, num_events, num_rc[2];
        size_t xs[MAX_RESERVATION_SIZE], ys[MAX_RESERVATION_SIZE];
        int response = 0;
        switch (op_buffer) {
          case '1':
            //FIX ME: various clients
          case '2':
            printf("Case 2: QUIT\n");
            //TODO: clear session_id
            read_session_id(req_pipe, &resp_session_id);
            
            if (pthread_mutex_lock(&sessions_mutex) != 0) {
              fprintf(stderr, "Error locking sessions mutex\n");
              //return 1;
            }

            sessions[session_id] = 0;
            sem_post(&semSessions);
            if (pthread_mutex_unlock(&sessions_mutex) != 0) {
              fprintf(stderr, "Error unlocking sessions mutex\n");
              //return 1;
            }
            //pthread_cond_signal(&cond);
            quit = 1;
            break;
          case '3':
            printf("Case 3: CREATE\n");

            read_create_request(req_pipe, &resp_session_id, &event_id, &num_rows, &num_cols);

            if (write_int(resp_pipe, ems_create(event_id, num_rows, num_cols))) {
              fprintf(stderr, "Error writing to responses pipe: %s\n", strerror(errno));
              //return 1;
            }  
            break;

          case '4':
            printf("Case 4: RESERVE\n");

            read_reserve_request(req_pipe, &resp_session_id, &event_id, &num_seats, xs, ys);

            if (write_int(resp_pipe, ems_reserve(event_id, num_seats, xs, ys))) {
              fprintf(stderr, "Error writing to responses pipe: %s\n", strerror(errno));
              //return 1;
            }       
            break;

          case '5':
            printf("Case 5: SHOW\n");

            read_show_request(req_pipe, &resp_session_id, &event_id);

            printf("show | seats address: %p\n", seats);

            response = ems_show(event_id, &num_rows, &num_cols, &seats);
            
            if (response) {
              if (write_int(resp_pipe, response)) {
                fprintf(stderr, "Error writing to responses pipe: %s\n", strerror(errno));
                //return 1;
              }
            } else {
              num_rc[0] = num_rows;
              num_rc[1] = num_cols;
              printf("show | num_rows: %zu num_cols: %zu seats address: %p\n", num_rc[0], num_rc[1], seats);
              for (size_t i = 0; i < num_rows*num_cols; i++) {
                printf("seats[%zu] = %u\n", i, seats[i]);
              }

              if (write_int(resp_pipe, response) || write_sizet_array(resp_pipe, num_rc, 2)
                  || write_uint_array(resp_pipe, seats, num_rows*num_cols)) {
                fprintf(stderr, "Error writing to responses pipe: %s\n", strerror(errno));
                //return 1;
              }
            }
            free(seats);
            break;
            
          case '6':
            printf("Case 6: LIST\n");

            read_session_id(req_pipe, &resp_session_id);
            
            response = ems_list_events(&num_events, &ids);

            if (response) {
              if (write_int(resp_pipe, response)) {
                fprintf(stderr, "Error writing to responses pipe: %s\n", strerror(errno));
                //return 1;
              } 
            } else {
              if (write_int(resp_pipe, response) || write_sizet(resp_pipe, num_events) 
                  || write_uint_array(resp_pipe, ids, num_events)) {
                fprintf(stderr, "Error writing to responses pipe: %s\n", strerror(errno));
                //return 1;
              }

            }
            free(ids);
            break;
        }
      }
      if (quit)
        break;
    }
    close(req_pipe);
    close(resp_pipe);
  }
}

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
  ClientQueue client_queue;
  init_queue(&client_queue);
  pthread_t tid[MAX_SESSION_COUNT];
  
  for (int i = 0; i < MAX_SESSION_COUNT; i++) {
    if (pthread_create(&tid[i], NULL, consumer_thread_fn, &client_queue) != 0) {
      fprintf(stderr, "Failed to create client thread: %s\n", strerror(errno));
      exit(1);
    }
  }
  
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

  printf("Server opened register pipe\n");

  sem_init(&semSessions, 0, MAX_SESSION_COUNT);
  
  while (1) {
    //TODO: Read from pipe
    //char buffer[82];
    //ssize_t bytes_read = read(reg_server, buffer, 81);
    char op_code;
    ssize_t bytes_read = read(reg_server, &op_code, sizeof(char));

    //printf("Bytes_read: %zd op_code: %c\n", bytes_read, op_code);

    if (bytes_read == 0) { // diferente neste caso como mudar?
      continue;
      // fprintf(stderr, "register pipe closed\n");
      // exit(EXIT_FAILURE);
    } else if (bytes_read == -1) {
      fprintf(stderr, "read failed: %s\n", strerror(errno));
      exit(EXIT_FAILURE);
    } else {
      /*
      char op_code = buffer[0];
      char req_pipe_path[40];
      char resp_pipe_path[40];
      */

      if (op_code != '1') {
        fprintf(stderr, "Operation code must be 1 for setup\n");
        exit(EXIT_FAILURE);
      }

      char req_path_buffer[40];
      char resp_path_buffer[40];
      bytes_read = read(reg_server, req_path_buffer, 40*sizeof(char));

      printf("Request pipe path: %s, bytes_read: %zd\n", req_path_buffer, bytes_read);

      if (bytes_read == 0) {
        fprintf(stderr, "register pipe closed\n");
        return 1;
      } else if (bytes_read == -1) {
        fprintf(stderr, "Error reading from register pipe: %s\n", strerror(errno));
        return 1;
      }

      bytes_read = read(reg_server, resp_path_buffer, 40*sizeof(char));

      printf("Responses pipe path: %s, bytes_read: %zd\n", resp_path_buffer, bytes_read);

      if (bytes_read == 0) {
        fprintf(stderr, "register pipe closed\n");
        return 1;
      } else if (bytes_read == -1) {
        fprintf(stderr, "Error reading from register pipe: %s\n", strerror(errno));
        return 1;
      }

      // Open requests pipe for reading
      // This waits for the client to open it for writing

      printf("Server stuck opening request pipe\n");

      size_t req_path_size = strlen(req_path_buffer) + strlen("../client/") + 1;
      size_t resp_path_size = strlen(resp_path_buffer) + strlen("../client/") + 1;

      char req_pipe_path[req_path_size];
      char resp_pipe_path[resp_path_size];

      snprintf(req_pipe_path, req_path_size, "../client/%s", req_path_buffer);
      snprintf(resp_pipe_path, resp_path_size, "../client/%s", resp_path_buffer);

      Client client;
      client.req_pipe_path = (char*)malloc(sizeof(char)*req_path_size);
      if (client.req_pipe_path == NULL) {
        perror("Memory allocation failed");
        return 1;
      }

      strcpy(client.req_pipe_path, req_pipe_path);

      client.resp_pipe_path = (char*)malloc(sizeof(char)*resp_path_size);
      if (client.resp_pipe_path == NULL) {
        perror("Memory allocation failed");
        return 1;
      }

      strcpy(client.resp_pipe_path, resp_pipe_path);

      enqueue(&client_queue, &client);

      // new_client.req_pipe = open(req_pipe_path, O_RDONLY);
      // if (new_client.req_pipe == -1) {
      //   fprintf(stderr, "open failed: %s\n", strerror(errno));
      //   exit(EXIT_FAILURE);
      // }

      // printf("Server opened request pipe, fd: %d\n", new_client.req_pipe);

      // // Open responses pipe for writing
      // // This waits for the client to open it for reading
      // new_client.resp_pipe = open(resp_pipe_path, O_WRONLY);
      // if (new_client.resp_pipe == -1) {
      //   fprintf(stderr, "open failed: %s\n", strerror(errno));
      //   exit(EXIT_FAILURE);
      // }

      // printf("Server opened responses pipe\n");

      // // TODO: generate session_id
      // for (int i = 0; i < MAX_SESSION_COUNT; i++) {
      //   pthread_mutex_lock(&sessions_mutex);
      //   if (sessions[i] == 0) {
      //     new_client.session_id = i;
      //     sessions[i] = 1;
      //     pthread_mutex_unlock(&sessions_mutex);
      //     break;
      //   }
      //   pthread_mutex_unlock(&sessions_mutex);
      // }

      // // Respond to the client with the corresponding session id
      // if (write_int(new_client.resp_pipe, new_client.session_id)) {
      //   fprintf(stderr, "Error writing to pipe: %s\n", strerror(errno));
      //   exit(EXIT_FAILURE); // ver pergunta 100 no piazza
      // }
    }

    //TODO: Write new client to the producer-consumer buffer
  }

  // int quit = 0;
  // while (1) {
  //   char op_buffer;
  //   ssize_t op_bytes_read = read(new_client.req_pipe, &op_buffer, sizeof(char));
  //   if (op_bytes_read == 0) {
  //     //TODO: clear session_id
  //     quit = 1;
  //   } else if (op_bytes_read == -1) {
  //     fprintf(stderr, "Read failed: %s\n", strerror(errno));
  //     exit(EXIT_FAILURE);
  //   } else {
  //     int session_id = 0;
  //     unsigned int event_id;
  //     unsigned int *seats = NULL, *ids = NULL;
  //     size_t num_rows, num_cols, num_seats, num_events, num_rc[2];
  //     size_t xs[MAX_RESERVATION_SIZE], ys[MAX_RESERVATION_SIZE];
  //     int response = 0;
  //     switch (op_buffer) {
  //       case '1':
  //         //FIX ME: various clients
  //       case '2':
  //         printf("Case 2: QUIT\n");
  //         //TODO: clear session_id
  //         read_session_id(new_client.req_pipe, &session_id);
  //         sessions[session_id] = 0; //mutex?
  //         quit = 1;
  //         break;

  //       case '3':
  //         printf("Case 3: CREATE\n");

  //         read_create_request(new_client.req_pipe, &session_id, &event_id, &num_rows, &num_cols);

  //         if (write_int(new_client.resp_pipe, ems_create(event_id, num_rows, num_cols))) {
  //           fprintf(stderr, "Error writing to responses pipe: %s\n", strerror(errno));
  //           return 1;
  //         }  
  //         break;

  //       case '4':
  //         printf("Case 4: RESERVE\n");

  //         read_reserve_request(new_client.req_pipe, &session_id, &event_id, &num_seats, xs, ys);

  //         if (write_int(new_client.resp_pipe, ems_reserve(event_id, num_seats, xs, ys))) {
  //           fprintf(stderr, "Error writing to responses pipe: %s\n", strerror(errno));
  //           return 1;
  //         }       
  //         break;

  //       case '5':
  //         printf("Case 5: SHOW\n");

  //         read_show_request(new_client.req_pipe, &session_id, &event_id);

  //         printf("show | seats address: %p\n", seats);

  //         response = ems_show(event_id, &num_rows, &num_cols, &seats);
          
  //         if (response) {
  //           if (write_int(new_client.resp_pipe, response)) {
  //             fprintf(stderr, "Error writing to responses pipe: %s\n", strerror(errno));
  //             return 1;
  //           }
  //         } else {
  //           num_rc[0] = num_rows;
  //           num_rc[1] = num_cols;
  //           printf("show | num_rows: %zu num_cols: %zu seats address: %p\n", num_rc[0], num_rc[1], seats);
  //           for (size_t i = 0; i < num_rows*num_cols; i++) {
  //             printf("seats[%zu] = %u\n", i, seats[i]);
  //           }

  //           if (write_int(new_client.resp_pipe, response) || write_sizet_array(new_client.resp_pipe, num_rc, 2)
  //               || write_uint_array(new_client.resp_pipe, seats, num_rows*num_cols)) {
  //             fprintf(stderr, "Error writing to responses pipe: %s\n", strerror(errno));
  //             return 1;
  //           }
  //         }
  //         free(seats);
  //         break;
          
  //       case '6':
  //         printf("Case 6: LIST\n");

  //         read_session_id(new_client.req_pipe, &session_id);
          
  //         response = ems_list_events(&num_events, &ids);

  //         if (response) {
  //           if (write_int(new_client.resp_pipe, response)) {
  //             fprintf(stderr, "Error writing to responses pipe: %s\n", strerror(errno));
  //             return 1;
  //           } 
  //         } else {
  //           if (write_int(new_client.resp_pipe, response) || write_sizet(new_client.resp_pipe, num_events) 
  //               || write_uint_array(new_client.resp_pipe, ids, num_events)) {
  //             fprintf(stderr, "Error writing to responses pipe: %s\n", strerror(errno));
  //             return 1;
  //           }

  //         }
  //         free(ids);
  //         break;
  //     }
  //   }
  //   if (quit)
  //     break;
  // }
  // close(new_client.req_pipe);
  // close(new_client.resp_pipe);


  for (int i = 0; i < MAX_SESSION_COUNT; i++) {
    pthread_join(tid[i], NULL); // erro
  }
  free_queue(&client_queue);
  //TODO: Close Server
  close(reg_server);
  sem_destroy(&semSessions);
  ems_terminate(); //apagar?
}