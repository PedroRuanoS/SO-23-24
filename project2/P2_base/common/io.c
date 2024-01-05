#include "io.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <stdio.h> // REMOVER
#include <fcntl.h>

int parse_uint(int fd, unsigned int *value, char *next) {
  char buf[16];

  int i = 0;
  while (1) {
    ssize_t read_bytes = read(fd, buf + i, 1);
    if (read_bytes == -1) {
      return 1;
    } else if (read_bytes == 0) {
      *next = '\0';
      break;
    }

    *next = buf[i];

    if (buf[i] > '9' || buf[i] < '0') {
      buf[i] = '\0';
      break;
    }

    i++;
  }

  unsigned long ul = strtoul(buf, NULL, 10);

  if (ul > UINT_MAX) {
    return 1;
  }

  *value = (unsigned int)ul;

  return 0;
}

int print_uint(int fd, unsigned int value) {
  char buffer[16];
  size_t i = 16;

  for (; value > 0; value /= 10) {
    buffer[--i] = '0' + (char)(value % 10);
  }

  if (i == 16) {
    buffer[--i] = '0';
  }

  while (i < 16) {
    ssize_t written = write(fd, buffer + i, 16 - i);
    if (written == -1) {
      return 1;
    }

    i += (size_t)written;
  }

  return 0;
}

int print_str(int fd, const char *str) {
  size_t len = strlen(str);
  while (len > 0) {
    ssize_t written = write(fd, str, len);
    if (written == -1) {
      return 1;
    }

    str += (size_t)written;
    len -= (size_t)written;
  }

  return 0;
}

int write_str(int fd, const char *str, size_t length) {
  ssize_t written = write(fd, str, sizeof(char) * length);
  if (written == -1) {
    return 1;
  }
  return 0;
}

int write_int(int fd, int value) {  
  ssize_t written = write(fd, &value, sizeof(int));

  printf("write_int: %zd\n", written);

  if (written == -1) {
    return 1;
  }
  return 0;
}

int write_uint(int fd, unsigned int value) { 
  ssize_t written = write(fd, &value, sizeof(unsigned int));

  printf("write_uint: %zd\n", written);

  if (written == -1) {
    return 1;
  }
  return 0;
}

int write_sizet(int fd, size_t value) {
  ssize_t written = write(fd, &value, sizeof(size_t));

  printf("write_sizet: %zd\n", written);

  if (written == -1) {
    return 1;
  }
  return 0;
}

int write_uint_array(int fd, unsigned int *array, size_t length) {
  ssize_t written = write(fd, array, sizeof(unsigned int) * length);

  printf("write_uint_array: %zd\n", written);

  if (written == -1) {
    return 1;
  }
  return 0;
}

int write_sizet_array(int fd, size_t *array, size_t length) {
  ssize_t written = write(fd, array, sizeof(size_t) * length);

  printf("write_sizet_array: %zd\n", written);

  if (written == -1) {
    return 1;
  }
  return 0;
}

int print_event_info(int fd, size_t num_rows, size_t num_cols, unsigned int *seats) {
  for (size_t i = 0; i < num_rows; i++) {
    for (size_t j = 0; j < num_cols; j++) {
      char buffer[16];
      sprintf(buffer, "%u", seats[i*num_cols + j]);

      printf("seats[%zu] = %u\n", i*num_cols + j, seats[i*num_cols + j]);

      if (print_str(fd, buffer)) {
        perror("Error writing to file descriptor");
        return 1;
      }

      if (j < num_cols - 1) {
        if (print_str(fd, " ")) {
        perror("Error writing to file descriptor");
        return 1;
        }
      }
    }

    if (print_str(fd, "\n")) {
      perror("Error writing to file descriptor");
      return 1;
    }
  }
  return 0;
}