/* 2ª Parte projeto SO 2023/24
 *
 * Grupo: 117
 * Alunos: Pedro Silveira (106642), Raquel Rodrigues (106322)
 */

#include <bits/pthreadtypes.h>
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
#include <signal.h>

#include "common/constants.h"
#include "common/io.h"
#include "operations.h"
#include "requests.h"
#include "prod_cons_queue.h"

static int sig_usr = 0;
pthread_mutex_t sessions_mutex = PTHREAD_MUTEX_INITIALIZER;
sem_t semSessions;
int sessions[MAX_SESSION_COUNT] = {0};

void sig_handler(int sig) {
  if (sig == SIGUSR1) {
    if (signal(SIGUSR1, sig_handler) == SIG_ERR) {
      fprintf(stderr, "Error establishing a signal handler\n");
      exit(EXIT_FAILURE);
    }

    // Flag to indicate the server to print events info.
    sig_usr = 1;
  }
}

void *consumer_thread_fn(void* arg) {
  sigset_t set;
  sigemptyset(&set);
  sigaddset(&set, SIGUSR1);

  if (pthread_sigmask(SIG_BLOCK, &set, NULL) != 0) {
    fprintf(stderr, "Error with pthread_sigmask\n");
    return NULL;
  }

  ClientQueue *queue = (ClientQueue*) arg;

  while (1) {
    Client new_client = dequeue(queue);
    int quit = 0;
    int session_id;
    int req_pipe;
    int resp_pipe;

    for (int i = 0; i < MAX_SESSION_COUNT; i++) {
      
      if (pthread_mutex_lock(&sessions_mutex) != 0) {
        fprintf(stderr, "Error locking sessions mutex\n");
        return NULL;
      }

      if (sessions[i] == 0) {
        session_id = i;
        sessions[i] = 1;

        if (pthread_mutex_unlock(&sessions_mutex) != 0) {
          fprintf(stderr, "Error unlocking sessions mutex\n");
          return NULL;
        }
        break;
      }
      if (pthread_mutex_unlock(&sessions_mutex) != 0) {
        fprintf(stderr, "Error unlocking sessions mutex\n");
        return NULL;
      }
    }

    req_pipe = open(new_client.req_pipe_path, O_RDONLY);
    if (req_pipe == -1) {
      fprintf(stderr, "Open failed: %s\n", strerror(errno));
      return NULL;
    }

    printf("Server opened request pipe, fd: %d\n", req_pipe);

    // Open responses pipe for writing
    // This waits for the client to open it for reading
    resp_pipe = open(new_client.resp_pipe_path, O_WRONLY);
    if (resp_pipe == -1) {
      fprintf(stderr, "Open failed: %s\n", strerror(errno));
      exit(EXIT_FAILURE);
    }

    printf("Server opened responses pipe\n");

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
      if (op_bytes_read == 0) { // client unavailable
        if (pthread_mutex_lock(&sessions_mutex) != 0) {
          fprintf(stderr, "Error locking sessions mutex\n");
          return NULL;
        }

        sessions[session_id] = 0;
        sem_post(&semSessions);
        if (pthread_mutex_unlock(&sessions_mutex) != 0) {
          fprintf(stderr, "Error unlocking sessions mutex\n");
          return NULL;
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
          case '2':
            printf("Case 2: QUIT\n");
<<<<<<< Updated upstream
            if (read_session_id(req_pipe, &resp_session_id)) {
              break;
            }
=======
            read_session_id(req_pipe, &resp_session_id);
>>>>>>> Stashed changes
            
            if (pthread_mutex_lock(&sessions_mutex) != 0) {
              fprintf(stderr, "Error locking sessions mutex\n");
              return NULL;
            }

            sessions[session_id] = 0;
            sem_post(&semSessions);
            if (pthread_mutex_unlock(&sessions_mutex) != 0) {
              fprintf(stderr, "Error unlocking sessions mutex\n");
              return NULL;
            }
            quit = 1;
            break;

          case '3':
            printf("Case 3: CREATE\n");

            if (read_create_request(req_pipe, &resp_session_id, &event_id, 
                &num_rows, &num_cols)) {
              break;
            }

            if (write_int(resp_pipe, ems_create(event_id, num_rows, num_cols))) {
              fprintf(stderr, "Error writing to responses pipe: %s\n", strerror(errno));
              return NULL;
            }  
            break;

          case '4':
            printf("Case 4: RESERVE\n");

            if (read_reserve_request(req_pipe, &resp_session_id, &event_id, 
                &num_seats, xs, ys)) {
              break;
            }

            if (write_int(resp_pipe, ems_reserve(event_id, num_seats, xs, ys))) {
              fprintf(stderr, "Error writing to responses pipe: %s\n", strerror(errno));
              return NULL;
            }       
            break;

          case '5':
            printf("Case 5: SHOW\n");

            if (read_show_request(req_pipe, &resp_session_id, &event_id)) {
              break;
            }

            printf("show | seats address: %p\n", seats);

            response = ems_show(event_id, &num_rows, &num_cols, &seats);
            
            if (response) {
              if (write_int(resp_pipe, response)) {
                fprintf(stderr, "Error writing to responses pipe: %s\n", strerror(errno));
                return NULL;
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
                return NULL;
              }
            }
            free(seats);
            break;
            
          case '6':
            printf("Case 6: LIST\n");

            if (read_session_id(req_pipe, &resp_session_id)) {
              break;
            }
            
            response = ems_list_events(&num_events, &ids);

            if (response) {
              if (write_int(resp_pipe, response)) {
                fprintf(stderr, "Error writing to responses pipe: %s\n", strerror(errno));
                return NULL;
              } 
            } else {
              if (write_int(resp_pipe, response) || write_sizet(resp_pipe, num_events) 
                  || write_uint_array(resp_pipe, ids, num_events)) {
                fprintf(stderr, "Error writing to responses pipe: %s\n", strerror(errno));
                return NULL;
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

  if (signal(SIGUSR1, sig_handler) == SIG_ERR) {
    fprintf(stderr, "Error establishing a signal handler: %s\n", strerror(errno));
    exit(EXIT_FAILURE);
  }

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
  if (mkfifo(pipe_path, 0640) != 0) {
    fprintf(stderr, "mkfifo failed: %s\n", strerror(errno));
    exit(EXIT_FAILURE);
  }

  // Open register pipe for reading
  // This waits for a client to open it for writing
  int reg_server = open(pipe_path, O_RDWR);
  if (reg_server == -1) {
    fprintf(stderr, "Open failed: %s\n", strerror(errno));
    exit(EXIT_FAILURE);
  }

  printf("Server opened register pipe\n");

  sem_init(&semSessions, 0, MAX_SESSION_COUNT);
  
  while (1) {
    sem_wait(&semSessions);
    //TODO: Read from pipe
    char op_code;
    ssize_t bytes_read = read(reg_server, &op_code, sizeof(char));

    // SIGUSR1 signal was sent
    if (sig_usr) {
      print_all_events(STDOUT_FILENO);
    }
    
    if (bytes_read == 0) {
      continue;

    } else if (bytes_read == -1) {
      fprintf(stderr, "Read failed: %s\n", strerror(errno));
      continue;
    } else {

      if (op_code != '1') {
        fprintf(stderr, "Operation code must be 1 for setup\n");
        continue;
      }

      char req_path_buffer[PATH_SIZE];
      char resp_path_buffer[PATH_SIZE];
      bytes_read = read(reg_server, req_path_buffer, PATH_SIZE*sizeof(char));

      printf("Request pipe path: %s, bytes_read: %zd\n", req_path_buffer, bytes_read);

      if (bytes_read == 0) {
        fprintf(stderr, "Register pipe closed\n");
        continue;
      } else if (bytes_read == -1) {
        fprintf(stderr, "Error reading from register pipe: %s\n", strerror(errno));
        continue;
      }

      bytes_read = read(reg_server, resp_path_buffer, PATH_SIZE*sizeof(char));

      printf("Responses pipe path: %s, bytes_read: %zd\n", resp_path_buffer, bytes_read);

      if (bytes_read == 0) {
        fprintf(stderr, "Register pipe closed\n");
        continue;
      } else if (bytes_read == -1) {
        fprintf(stderr, "Error reading from register pipe: %s\n", strerror(errno));
        continue;
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
        continue;
      }

      strcpy(client.req_pipe_path, req_pipe_path);

      client.resp_pipe_path = (char*)malloc(sizeof(char)*resp_path_size);
      if (client.resp_pipe_path == NULL) {
        perror("Memory allocation failed");
        continue;
      }

      strcpy(client.resp_pipe_path, resp_pipe_path);

      enqueue(&client_queue, &client);
    }
  }

  for (int i = 0; i < MAX_SESSION_COUNT; i++) {
    if (pthread_join(tid[i], NULL) != 0) {
      perror("Error joining threads")
      exit(EXIT_FAILURE);
    }
  }
  free_queue(&client_queue);
  close(reg_server);
  sem_destroy(&semSessions);
  ems_terminate();
}