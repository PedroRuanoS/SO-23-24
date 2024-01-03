#ifndef REQUESTS_H
#define REQUESTS_H

#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#include "common/constants.h"

void read_session_id(int req_pipe, int *session_id);
void read_event_id(int req_pipe, unsigned int *event_id);
void read_create_request(int req_pipe, int *session_id, unsigned int *event_id, size_t *num_rows, size_t *num_cols);
void read_reserve_request(int req_pipe, int *session_id, unsigned int *event_id, size_t *num_seats, size_t *xs, size_t *ys);
void read_show_request(int req_pipe, int *session_id, unsigned int *event_id);

#endif  // REQUESTS_H