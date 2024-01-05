#ifndef COMMON_IO_H
#define COMMON_IO_H

#include <stddef.h>

/// Parses an unsigned integer from the given file descriptor.
/// @param fd The file descriptor to read from.
/// @param value Pointer to the variable to store the value in.
/// @param next Pointer to the variable to store the next character in.
/// @return 0 if the integer was read successfully, 1 otherwise.
int parse_uint(int fd, unsigned int *value, char *next);

/// Prints an unsigned integer to the given file descriptor.
/// @param fd The file descriptor to write to.
/// @param value The value to write.
/// @return 0 if the integer was written successfully, 1 otherwise.
int print_uint(int fd, unsigned int value);

/// Writes a string to the given file descriptor.
/// @param fd The file descriptor to write to.
/// @param str The string to write.
/// @return 0 if the string was written successfully, 1 otherwise.
int print_str(int fd, const char *str);

int write_str(int fd, const char *str, size_t length);

int write_int(int fd, int value);

int write_uint(int fd, unsigned int value);

int write_sizet(int fd, size_t value);

int write_uint_array(int fd, unsigned int *array, size_t length);

int write_sizet_array(int fd, size_t *array, size_t length);

int print_event_info(int fd, size_t num_rows, size_t num_cols, unsigned int *seats);

#endif  // COMMON_IO_H
