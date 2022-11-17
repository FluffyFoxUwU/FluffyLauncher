/*
#include <stdlib.h>
#include <sys/select.h>
#include <errno.h>
#include <unistd.h>
#include <stdatomic.h>

#include "io_threads.h"
#include "vec.h"

struct custom_file_descriptor {
  // If this descriptor being handled one would win and
  // other threads go back waiting with poll
  atomic_bool isBeingHandled;
  int fd;
  
  struct file_operations* ops; 
};

enum control_code {
  CONTROL_UNSET,
  CONTROL_UPDATE,
};

static pthread_t* threads = NULL;
static vec_int_t descriptorsToWatch;
static int controlWriteFD = -1;
static int controlReadFD = -1;

int io_threads_start(int threadCount) {
  vec_init(&descriptorsToWatch);
  
  int res = 0;
  threads = calloc(threadCount, sizeof(*threads));
  if (!threads)
    return -ENOMEM;
  
  int controlPipe[2] = {0, 0};
  res = pipe(controlPipe);
  if (res < 0)
    goto fail_to_create_control_pipe;
  controlWriteFD = controlPipe[0];
  controlReadFD = controlPipe[1];
  
fail_to_create_control_pipe:
  if (res < 0)
    io_threads_stop();
  return res;
}

void io_threads_stop() {
  vec_deinit(&descriptorsToWatch);
  free(threads);
}
*/