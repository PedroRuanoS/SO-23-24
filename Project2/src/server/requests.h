#ifndef REQUESTS_H
#define REQUESTS_H

#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#include "common/constants.h"

/// Reads a session id.
/// @param req_pipe Pipe to read from.
/// @param session_id Pointer to the id of the session to read.
/// @return 0 if the session id was read successfully, 1 otherwise.
int read_session_id(int req_pipe, int *session_id);

/// Reads an event id.
/// @param req_pipe Pipe to read from.
/// @param event_id Pointer to the id of the event to read.
/// @return 0 if the event id was read successfully, 1 otherwise.
int read_event_id(int req_pipe, unsigned int *event_id);

/// Reads a create request.
/// @param req_pipe Pipe to read from.
/// @param session_id Pointer to the id of the session to read.
/// @param event_id Pointer to the id of the event to read.
/// @param num_rows Pointer to the number of rows of the event to read.
/// @param num_cols Pointer to the number of columns of the event to read.
/// @return 0 if the request was read successfully, 1 otherwise.
int read_create_request(int req_pipe, int *session_id, unsigned int *event_id, size_t *num_rows, size_t *num_cols);

/// Reads a reserve request.
/// @param req_pipe Pipe to read from.
/// @param session_id Pointer to the id of the session to read.
/// @param event_id Pointer to the id of the event to read.
/// @param num_seats Pointer to the number of seats of the event to read.
/// @param xs Array of rows of the seats to read.
/// @param ys Array of columns of the seats to read.
/// @return 0 if the request was read successfully, 1 otherwise.
int read_reserve_request(int req_pipe, int *session_id, unsigned int *event_id, size_t *num_seats, size_t *xs, size_t *ys);

/// Reads a show request.
/// @param req_pipe Pipe to read from.
/// @param session_id Pointer to the id of the session to read.
/// @param event_id Pointer to the id of the event to read.
/// @return 0 if the request was read successfully, 1 otherwise.
int read_show_request(int req_pipe, int *session_id, unsigned int *event_id);

#endif  // REQUESTS_H