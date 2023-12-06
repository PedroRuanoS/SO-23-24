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

int main(int argc, char *argv[]) {
  unsigned int state_access_delay_ms = STATE_ACCESS_DELAY_MS;

  if (argc < 2) // no directory argument specified
    return 1;

  if (argc > 2) {
    char *endptr;
    unsigned long int delay = strtoul(argv[2], &endptr, 10);

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

  const char *dir_path = argv[1];
  size_t dir_path_len = strlen(dir_path);
  DIR *target_dir = opendir(dir_path);
  
  if (target_dir == NULL) {
    fprintf(stderr, "Failed to open the directory\n");
    return 1;
  }

  struct dirent *entry;
  char *file_path;

  while ((entry = readdir(target_dir)) != NULL) {
    unsigned int event_id, delay;
    size_t num_rows, num_columns, num_coords;
    size_t xs[MAX_RESERVATION_SIZE], ys[MAX_RESERVATION_SIZE];
    enum Command cmd = 0;

    // Exclude "." and ".." entries
    if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
        continue;
    }
    // allocate space for the file path including the extra slash
    file_path = (char*)malloc(sizeof(char)*(dir_path_len + strlen(entry->d_name) + 2));
    
    if (file_path == NULL) {
        fprintf(stderr, "Memory allocation failed\n");
        return 1;
    }

    // Copy the directory path to file_path
    strcpy(file_path, dir_path);    
    strcat(file_path, "/");
    // Concatenate the file name onto file_path
    strcat(file_path, entry->d_name);

    printf("%s\n", file_path); // REMOVER, PRINT PARA TESTAR

    int fd_jobs = open(file_path, O_RDONLY); //file_path não é const char*, causa problemas?

    if (fd_jobs < 0) {
      fprintf(stderr, "Error opening file\n");
      return 1;
    }

    // Find the position of the dot in the original file name
    //const char *dotPosition = strrchr(file_path, '.');
    // Calculate the length of the prefix (characters before the dot)
    // size_t prefixLength = (size_t) (dotPosition - file_path); //cast manhoso?
    // char *output_file_path = "";
    // strncpy(output_file_path, file_path, prefixLength);
    // strcat(output_file_path, ".out");

    // int fd_out = open(output_file_path, O_CREAT | O_TRUNC | O_WRONLY , S_IRUSR | S_IWUSR);

    // if (fd_out < 0) {
    //   fprintf(stderr, "Error opening file\n");
    //   return 1;
    // }

    printf("> ");
    fflush(stdout);

    while (cmd != EOC) {
      switch (cmd = get_next(fd_jobs)) {
      case CMD_CREATE:
        if (parse_create(fd_jobs, &event_id, &num_rows, &num_columns) != 0) {
          fprintf(stderr, "Invalid command. See HELP for usage\n");
          continue;
        }

        if (ems_create(event_id, num_rows, num_columns)) {
          fprintf(stderr, "Failed to create event\n");
        }

        break;

      case CMD_RESERVE:
        num_coords = parse_reserve(fd_jobs, MAX_RESERVATION_SIZE, &event_id, xs, ys);

        if (num_coords == 0) {
          fprintf(stderr, "Invalid command. See HELP for usage\n");
          continue;
        }

        if (ems_reserve(event_id, num_coords, xs, ys)) {
          fprintf(stderr, "Failed to reserve seats\n");
        }

        break;

      case CMD_SHOW:
        if (parse_show(fd_jobs, &event_id) != 0) {
          fprintf(stderr, "Invalid command. See HELP for usage\n");
          continue;
        }

        if (ems_show(event_id)) {
          fprintf(stderr, "Failed to show event\n");
        }

        break;

      case CMD_LIST_EVENTS:
        if (ems_list_events()) {
          fprintf(stderr, "Failed to list events\n");
        }

        break;

      case CMD_WAIT:
        if (parse_wait(fd_jobs, &delay, NULL) == -1) {  // thread_id is not implemented
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

      case CMD_BARRIER:  // Not implemented
      case CMD_EMPTY:
        break;

      case EOC:
        break; //continue?
      }      
    }
    close(fd_jobs); //if close != 0 error closing
      
    //close(fd_out);
    free(file_path);    
  }
  //ems_terminate();
  closedir(target_dir);
  return 0;
}
