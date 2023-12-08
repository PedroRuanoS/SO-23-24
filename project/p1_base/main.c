#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include <errno.h>
#include <string.h>

#include "constants.h"
#include "operations.h"
#include "parser.h"

int is_job_file(const char *filename) {
  const char *dot = strrchr(filename, '.');
  return dot && !strcmp(dot, ".job");
}

int main(int argc, char *argv[]) {
  unsigned int state_access_delay_ms = STATE_ACCESS_DELAY_MS;

  if (argc < 2) {
    fprintf(stderr, "Usage: %s <directory_path>\n", argv[0]);
    return 1;
  }

  if (argc > 2) {
    char *endptr;
    unsigned long int delay = strtoul(argv[2], &endptr, 10);

    if (*endptr != '\0' || delay > UINT_MAX) {
      fprintf(stderr, "Invalid delay value\n");
      return 1;
    }

    state_access_delay_ms = (unsigned int)delay;
  }

  if (ems_init(state_access_delay_ms)) {
    fprintf(stderr, "Failed to initialize EMS\n");
    return 1;
  }

  DIR *target_dir = opendir(argv[1]);
  if (target_dir == NULL) {
    perror("Failed to open directory");
    return 1;
  }

  struct dirent *entry;
  while ((entry = readdir(target_dir)) != NULL) {
    if (is_job_file(entry->d_name)) {
      char job_file_path[PATH_MAX];
      snprintf(job_file_path, PATH_MAX, "%s/%s", argv[1], entry->d_name);
      int fd_job = open(job_file_path, O_RDONLY);
      if (fd_job < 0) {
        perror("Error opening job file");
        continue;
      }

      char out_file_path[PATH_MAX];
      snprintf(out_file_path, PATH_MAX, "%s/%s.out", argv[1], entry->d_name);
      int fd_out = open(out_file_path, O_CREAT | O_TRUNC | O_WRONLY, 0644);
      if (fd_out < 0) {
        perror("Error opening out file");
        close(fd_job);
        continue;
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
          if (ems_list_events()) {
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
          
    }
  }  
  closedir(target_dir);
  ems_terminate();
  return 0;
  
}
