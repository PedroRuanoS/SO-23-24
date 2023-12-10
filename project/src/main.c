#include <limits.h>
#include <signal.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include <errno.h>
#include <string.h>
#include <sys/wait.h>

#include "constants.h"
#include "operations.h"
#include "parser.h"

int is_job_file(const char *filename) {
  const char *dot = strrchr(filename, '.');
  return dot && !strcmp(dot, ".jobs");
}

int main(int argc, char *argv[]) {
  unsigned int state_access_delay_ms = STATE_ACCESS_DELAY_MS;
  //int max_proc = 10; FIX ME o que por default value?

  if (argc < 3/*4*/) {
    fprintf(stderr, "Usage: %s <directory_path> <MAX_PROC> <MAX_THREADS>\n", argv[0]);
    return 1;
  }

  if (argc > 3/*4*/) {
    char *endptr;
    unsigned long int delay = strtoul(argv[3/*4*/], &endptr, 10);

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
  //int max_threads = atoi(argv[3]);

  DIR *target_dir = opendir(argv[1]);
  if (target_dir == NULL) {
    perror("Failed to open directory");
    return 1;
  }

  pid_t child_pids[max_proc];
  size_t num_children = 0;

  struct dirent *entry;
  while ((entry = readdir(target_dir)) != NULL) {
    if (is_job_file(entry->d_name)) {
      if (num_children == (size_t) max_proc) {
        int status;
        pid_t terminated_pid = wait(&status);
        if (terminated_pid == -1) {
          perror("Error waiting for child process to terminate\n");
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
      pid_t pid = fork();
      if (pid == -1) {
        perror("Error in fork");
        return 1;

      } else if (pid == 0) {
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

        enum Command cmd;
        while ((cmd = get_next(fd_job)) != EOC) {
          unsigned int event_id, delay;
          size_t num_rows, num_columns, num_coords;
          size_t xs[MAX_RESERVATION_SIZE], ys[MAX_RESERVATION_SIZE];
          switch (cmd) {
          case CMD_CREATE: 
            if (parse_create(fd_job, &event_id, &num_rows, &num_columns) != 0) {
              fprintf(stderr, "Invalid command. See HELP for usage\n");
              continue;
            }

            if (ems_create(event_id, num_rows, num_columns)) {
              fprintf(stderr, "Failed to create event\n");
            }

            break;
      
          case CMD_RESERVE: 
            num_coords = parse_reserve(fd_job, MAX_RESERVATION_SIZE, &event_id, xs, ys);

            if (num_coords == 0) {
              fprintf(stderr, "Invalid command. See HELP for usage\n");
              continue;
            }

            if (ems_reserve(event_id, num_coords, xs, ys)) {
              fprintf(stderr, "Failed to reserve seats\n");
            }

            break;
          
          case CMD_SHOW: 
            if (parse_show(fd_job, &event_id) != 0) {
              fprintf(stderr, "Invalid command. See HELP for usage\n");
              continue;
            }

            if (ems_show(event_id, fd_out)) {
              fprintf(stderr, "Failed to show event\n");
            }

            break;
          

          case CMD_LIST_EVENTS: 
            if (ems_list_events(fd_out)) {
              fprintf(stderr, "Failed to list events\n");
            }

            break;
          
          case CMD_WAIT: 
            if (parse_wait(fd_job, &delay, NULL) == -1) {
              fprintf(stderr, "Invalid command. See HELP for usage\n");
              continue;
            }
              
            if (delay > 0) {
              printf("Waiting...\n");
              ems_wait(delay);
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
              "  BARRIER\n"                      // Not implemented
              "  HELP\n");

            break;

          case CMD_BARRIER:
          case CMD_EMPTY:
            break;
          
          case EOC:
            break;
          } 
        }

        close(fd_job);
        close(fd_out);
        exit(0);

      } else {
        child_pids[num_children++] = pid;
      }            
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
ems_terminate();
return 0;  
}
