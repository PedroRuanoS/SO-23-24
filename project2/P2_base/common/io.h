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

/// Writes a string to the given pipe.
/// @param fd The file descriptor of the pipe to write to.
/// @param str The string to write.
/// @param length Length of the char array.
/// @return 0 if the string was written successfully, 1 otherwise.
int write_str(int fd, const char *str, size_t length);

/// Writes an integer to the given pipe.
/// @param fd The file descriptor of the pipe to write to.
/// @param value The integer variable to write.
/// @return 0 if the integer was written successfully, 1 otherwise.
int write_int(int fd, int value);

/// Writes an unsigned integer to the given pipe.
/// @param fd The file descriptor of the pipe to write to.
/// @param value The unsigned integer variable to write.
/// @return 0 if the unsigned int was written successfully, 1 otherwise.
int write_uint(int fd, unsigned int value);

/// Writes a variable of type size_t to the given pipe.
/// @param fd The file descriptor of the pipe to write to.
/// @param value The size_t variable to write.
/// @return 0 if the size_t variable was written successfully, 1 otherwise.
int write_sizet(int fd, size_t value);

/// Writes an unsigned int array to the given pipe.
/// @param fd The file descriptor of the pipe to write to.
/// @param array The unsigned int array to write.
/// @param length Length of the unsigned int array.
/// @return 0 if the array was written successfully, 1 otherwise.
int write_uint_array(int fd, unsigned int *array, size_t length);

/// Writes an array of type size_t to the given pipe.
/// @param fd The file descriptor of the pipe to write to.
/// @param array The size_t array to write.
/// @param length Length of the size_t array.
/// @return 0 if the array was written successfully, 1 otherwise.
int write_sizet_array(int fd, size_t *array, size_t length);

/// Prints the seats of an event.
/// @param fd The file descriptor to write to.
/// @param num_rows Number of rows of the event.
/// @param num_cols Number of columns of the event.
/// @param seats Array of seats.
/// @return 0 if the seats were written successfully, 1 otherwise.
int print_event_info(int fd, size_t num_rows, size_t num_cols, unsigned int *seats);

#endif  // COMMON_IO_H
