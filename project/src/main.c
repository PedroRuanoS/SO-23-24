#include <limits.h>
#include <signal.h>
#include <stddef.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include <errno.h>
#include <string.h>
#include <sys/wait.h>
#include <pthread.h>

#include "constants.h"
#include "operations.h"
#include "parser.h"

typedef struct {
  enum Command cmd;
  unsigned int event_id, delay;
  int thread_id;
  size_t num_rows, num_columns, num_coords;
  size_t xs[MAX_RESERVATION_SIZE], ys[MAX_RESERVATION_SIZE];
} command;

typedef struct queueNode {
  command cmd;
  struct queueNode *next;
} QueueNode;

typedef struct {
  QueueNode *head;
  QueueNode *tail;
  pthread_mutex_t mutex;
  pthread_cond_t cond;
  pthread_cond_t barrier_cond;
  int fd;
  bool terminate;  // Flag to signal termination
  bool barrier_active;
  unsigned int *thread_wait;
} CommandQueue;

typedef struct {
  int thread_id;
  CommandQueue *queue;
} ThreadArg;

void init_queue(CommandQueue *queue, int fd, size_t max_threads) {
  queue->head = queue->tail = NULL;
  pthread_mutex_init(&queue->mutex, NULL);
  pthread_cond_init(&queue->cond, NULL);
  pthread_cond_init(&queue->barrier_cond, NULL);
  queue->fd = fd;
  queue->terminate = false;
  queue->barrier_active = false;
  queue->thread_wait = (unsigned int*)malloc(max_threads * sizeof(unsigned int));
  if (queue->thread_wait == NULL) {
    fprintf(stderr, "Error: Memory allocation failed\n");
    exit(1);
  }
  for (size_t i = 0; i < max_threads; i++) {
        queue->thread_wait[i] = 0;
    }
}

void enqueue(CommandQueue *queue, command *cmd) {
  printf("Enqueue | Command: %d | Fd: %d\n", cmd->cmd, queue->fd);
  
  pthread_mutex_lock(&queue->mutex);

  QueueNode *newNode = (QueueNode*)malloc(sizeof(QueueNode));
  if (newNode == NULL) {
    fprintf(stderr, "Error: Memory allocation failed\n");
    exit(1);
  }

  newNode->cmd = *cmd;
  newNode->next = NULL;

  if (queue->tail == NULL) {
    queue->head = queue->tail = newNode;
  } else {
    queue->tail->next = newNode;
    queue->tail = newNode;
  }
///////////
  QueueNode *current = queue->head;
  while (current) {
    printf("QueueNode: %d\n", current->cmd.cmd);
    current = current->next;
  }
////////////
  pthread_cond_signal(&queue->cond);
  pthread_mutex_unlock(&queue->mutex);
}

command dequeue(CommandQueue *queue) {
  pthread_mutex_lock(&queue->mutex);

  while (queue->head == NULL && !queue->terminate) {
    // Wait for a command to be enqueued or termination signal
    printf("Waiting for commands to be enqueued\n");
    pthread_cond_wait(&queue->cond, &queue->mutex);

  }
/////////////
  command cmd_args = {.cmd = CMD_EMPTY, .delay = 0};

  
  if (queue->head != NULL) {
    printf("queue head not NULL \n");

    QueueNode *temp = queue->head;
    cmd_args = temp->cmd;

    printf("cmd: %d\n", cmd_args.cmd);

    queue->head = temp->next;
    if (queue->head == NULL) {
      queue->tail = NULL;
    }

    free(temp);
  }
  else {
    printf("queue head NULL\n");
  }

  
  if (queue->head == NULL && queue->barrier_active) {
    // notify main thread once all commands preceding barrier have been executed
    pthread_cond_signal(&queue->barrier_cond);
  }

  pthread_mutex_unlock(&queue->mutex);
  return cmd_args;
}

void free_queue(CommandQueue *queue) {
  if (!queue) return;
  QueueNode* current = queue->head;
  while (current) {
    QueueNode* temp = current;
    current = current->next;

    free(temp);
  }
  free(queue->thread_wait);
  pthread_mutex_destroy(&queue->mutex);
  pthread_cond_destroy(&queue->cond);
  pthread_cond_destroy(&queue->barrier_cond);
}

int is_job_file(const char *filename) {
  const char *dot = strrchr(filename, '.');
  return dot && !strcmp(dot, ".jobs");
}

void *command_thread_fn(void* arg) {
  ThreadArg *argument = (ThreadArg*) arg;
  CommandQueue *queue = argument->queue;
  int thread_id = argument->thread_id;

  while (true) {
    command cmd_args = dequeue(queue);

    printf("Dequeue | Command: %d | Fd: %d\n", cmd_args.cmd, queue->fd);


    pthread_mutex_lock(&queue->mutex);
    if (cmd_args.delay > 0 && queue->thread_wait[thread_id] != 0) {
      ems_wait(queue->thread_wait[thread_id]);
      queue->thread_wait[thread_id] = 0;
    }
    pthread_mutex_unlock(&queue->mutex);

    switch (cmd_args.cmd) {
      case CMD_CREATE:
        printf("CREATE\n");          
        if (ems_create(cmd_args.event_id, cmd_args.num_rows, cmd_args.num_columns)) {
          fprintf(stderr, "Failed to create event\n");
        }

        break;

      case CMD_RESERVE:
        printf("RESERVE\n"); 
        if (ems_reserve(cmd_args.event_id, cmd_args.num_coords, cmd_args.xs, cmd_args.ys)) {
          fprintf(stderr, "Failed to reserve seats\n");
        }

        break;
      
      case CMD_SHOW:
        printf("SHOW\n");  
        if (ems_show(cmd_args.event_id, queue->fd)) {
          fprintf(stderr, "Failed to show event\n");
        }

        break;      

      case CMD_LIST_EVENTS:
        printf("LIST\n");  
        if (ems_list_events(queue->fd)) {
          fprintf(stderr, "Failed to list events\n");
        }

        break;
      
      case CMD_WAIT: 
        queue->thread_wait[cmd_args.thread_id] = cmd_args.delay;

        break;

      case CMD_INVALID:
      case CMD_HELP:
      case CMD_BARRIER:
        printf("BARRIER\n"); 
        break;
      case CMD_EMPTY:
      case EOC:
      default:
        break;
    }
    // Check for termination
    pthread_mutex_lock(&queue->mutex);
    if (queue->terminate && queue->head == NULL) {
      printf("terminate thread\n");
      pthread_mutex_unlock(&queue->mutex);
      break;
    }
    pthread_mutex_unlock(&queue->mutex);  
  } 
  return NULL;
}

int main(int argc, char *argv[]) {
  unsigned int state_access_delay_ms = STATE_ACCESS_DELAY_MS;

  if (argc < 4) {
    fprintf(stderr, "Usage: %s <directory_path> <MAX_PROC> <MAX_THREADS>\n", argv[0]);
    return 1;
  }

  if (argc > 4) {
    char *endptr;
    unsigned long int delay = strtoul(argv[4], &endptr, 10);

    if (*endptr != '\0' || delay > UINT_MAX) {
      fprintf(stderr, "Invalid delay value or value too large\n");
      return 1;
    }

    state_access_delay_ms = (unsigned int)delay;
  }

  if (ems_init(state_access_delay_ms)) {
    fprintf(stderr, "Failed to initialize EMS\n");
    return 1;
  }

  int max_proc = atoi(argv[2]);
  if (max_proc <= 0) {
    fprintf(stderr, "Invalid MAX_PROC value\n");
    return 1;
  }
  int max_threads = atoi(argv[3]);

  DIR *target_dir = opendir(argv[1]);
  if (target_dir == NULL) {
    perror("Failed to open directory");
    return 1;
  }

  pid_t child_pids[max_proc];
  size_t num_children = 0;

  struct dirent *entry;
  while ((entry = readdir(target_dir)) != NULL) {
    if (!is_job_file(entry->d_name))
      continue;
    if (num_children == (size_t) max_proc) {
      int status;
      pid_t terminated_pid = wait(&status);
      if (terminated_pid == -1) {
        perror("Error waiting for child process to terminate\n");
        exit(1);
      } else {
        if (WIFEXITED(status)) {
          printf("Process %d terminated with status %d\n", terminated_pid, WEXITSTATUS(status));
        } else if (WIFSIGNALED(status)) {
          printf("Process %d terminated by signal %d\n", terminated_pid, WTERMSIG(status));
        } else {
          printf("Process %d terminated abnormally\n", terminated_pid);
        }
      }
      for (size_t i = 0; i < num_children; i++) {
        if (child_pids[i] == terminated_pid) {
          child_pids[i] = child_pids[num_children - 1];
          num_children--;
          break;
        }
      }
    }
    char job_file_path[PATH_MAX];
    snprintf(job_file_path, PATH_MAX, "%s/%s", argv[1], entry->d_name);
    int fd_job = open(job_file_path, O_RDONLY);
    if (fd_job < 0) {
      perror("Error opening job file");
      exit(1);
    }

    char out_file_path[PATH_MAX];
    snprintf(out_file_path, PATH_MAX, "%s/%.*s.out", argv[1],
      (int)(strlen(entry->d_name) - 5), entry->d_name);
    int fd_out = open(out_file_path, O_CREAT | O_TRUNC | O_WRONLY, S_IRUSR | S_IWUSR);
    if (fd_out < 0) {
      perror("Error opening out file");
      close(fd_job);
      exit(1);
    }

    pid_t pid = fork();
    if (pid == -1) {
      perror("Error in fork");
      return 1;
    } else if (pid == 0) {
      ThreadArg args[max_threads];
      CommandQueue commandQueue;
      init_queue(&commandQueue, fd_out, (size_t) max_threads);
      pthread_t tid[max_threads];
      enum Command cmd;

      for (int i = 0; i < max_threads; i++) {
        args[i].thread_id = i;
        args[i].queue = &commandQueue;

        if (pthread_create(&tid[i], NULL, command_thread_fn, &args[i]) != 0) {
          fprintf(stderr, "Failed to create command thread: %s\n", strerror(errno));
          exit(1);
        }
/////////
        printf("Fd_job: %d | File: %s | Fd_out: %d | Thread created: %d\n", fd_job, out_file_path, fd_out, i);
      }

      while ((cmd = get_next(fd_job)) != EOC) {
        
        unsigned int event_id, delay;
        int thread_id;
        size_t num_rows, num_columns, num_coords;
        size_t xs[MAX_RESERVATION_SIZE], ys[MAX_RESERVATION_SIZE];
        command cmd_args = {.cmd = cmd};
/////////
        printf("%s | Command being parsed: %d\n", out_file_path, cmd);
/////////          

        switch (cmd) {
          case CMD_CREATE: 
            if (parse_create(fd_job, &event_id, &num_rows, &num_columns) != 0) {
              fprintf(stderr, "Invalid command. See HELP for usage\n");
              continue;
            }
            cmd_args.event_id = event_id;  
            cmd_args.num_rows = num_rows;
            cmd_args.num_columns = num_columns;      
            enqueue(&commandQueue, &cmd_args);
            break;

          case CMD_RESERVE: 
            num_coords = parse_reserve(fd_job, MAX_RESERVATION_SIZE, &event_id, xs, ys);

            if (num_coords == 0) {
              fprintf(stderr, "Invalid command. See HELP for usage\n");
              continue;
            }
            
            cmd_args.event_id = event_id;
            cmd_args.num_coords = num_coords;
            for (size_t i = 0; i < MAX_RESERVATION_SIZE; ++i) {
              cmd_args.xs[i] = xs[i];
              cmd_args.ys[i] = ys[i];
            }              
            enqueue(&commandQueue, &cmd_args);
            break;
          
          case CMD_SHOW: 
            if (parse_show(fd_job, &event_id) != 0) {
              fprintf(stderr, "Invalid command. See HELP for usage\n");
              continue;
            }

            cmd_args.event_id = event_id;
            enqueue(&commandQueue, &cmd_args);
            break;
          

          case CMD_LIST_EVENTS:
            enqueue(&commandQueue, &cmd_args);
            break;
          
          case CMD_WAIT: 
            if (parse_wait(fd_job, &delay, &thread_id) == -1) {
              fprintf(stderr, "Invalid command. See HELP for usage\n");
              continue;
            }
            if (thread_id == -1) {
              ems_wait(delay);
            } else {
              cmd_args.thread_id = thread_id;
              cmd_args.delay = delay;
              enqueue(&commandQueue, &cmd_args);
            }
            
            break;
          
          case CMD_INVALID:
            fprintf(stderr, "Invalid command. See HELP for usage\n");
            break;

          case CMD_HELP:
            printf(
              "Available commands:\n"
              "  CREATE <event_id> <num_rows> <num_columns>\n"
              "  RESERVE <event_id> [(<x1>,<y1>) (<x2>,<y2>) ...]\n"
              "  SHOW <event_id>\n"
              "  LIST\n"
              "  WAIT <delay_ms> [thread_id]\n"  // thread_id is not implemented
              "  BARRIER\n"                      
              "  HELP\n");

            break;

          case CMD_BARRIER: 
            pthread_mutex_lock(&commandQueue.mutex);
            commandQueue.barrier_active = true;

            // wait until other threads are finished with the previous commands
            while (commandQueue.head != NULL) {
              pthread_cond_wait(&commandQueue.barrier_cond, &commandQueue.mutex);
            }

            commandQueue.barrier_active = false;
            pthread_mutex_unlock(&commandQueue.mutex);
            break;

          case CMD_EMPTY:
            break;
          
          case EOC:
            break;
        }          
      }
      // Signal termination and wake up waiting threads
      pthread_mutex_lock(&commandQueue.mutex);
      commandQueue.terminate = true;
      pthread_cond_broadcast(&commandQueue.cond);
      pthread_mutex_unlock(&commandQueue.mutex);

      for (int i = 0; i < max_threads; i++) {
        pthread_join(tid[i], NULL);
        printf("Thread joined: %d\n", i);
      }   
      free_queue(&commandQueue);

      ems_terminate();
      close(fd_job);
      close(fd_out);
      exit(0);
    } else {
      child_pids[num_children++] = pid;
    } 
  } 

for (size_t i = 0; i < num_children; i++) {
  int status;
  pid_t terminated_pid;
  terminated_pid = waitpid(child_pids[i], &status, 0);
  
  if (terminated_pid == -1) {
    fprintf(stderr, "Error waiting for child process %d to terminate\n", child_pids[i]);
    
  }

  if (WIFEXITED(status)) {
    printf("Process %d terminated with status %d\n", terminated_pid, WEXITSTATUS(status));
  } else if (WIFSIGNALED(status)) {
    printf("Process %d killed by signal %d\n", terminated_pid, WTERMSIG(status));
  } else {
    printf("Process %d terminated abnormally\n", terminated_pid);
  }
}

closedir(target_dir);
return 0;  
}
