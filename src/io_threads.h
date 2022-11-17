#ifndef _headers_1667913843_FluffyLauncher_io_thread
#define _headers_1667913843_FluffyLauncher_io_thread

// For the purpose of emulating
// file descriptors with custom read/write

#include <stddef.h>
#include <sys/types.h>

struct file_operations {
  // Same prototype as read/write/close in unistd.h
  ssize_t (*read)(struct file_operations* self, void* buffer, size_t readSize);
  ssize_t (*write)(struct file_operations* self, const void* buffer, size_t writeSize);
  int (*close)(struct file_operations* self);
};

// Return 0 on success
// Errors:
// -ENOMEM: Not enough memory
// -EINVAL: Attempt to double start
[[nodiscard]]
int io_threads_start(int threadCount);
void io_threads_stop();

// Return normal fd of type being faked
[[nodiscard]]
int io_fake_file_fd(struct file_operations* ops);

#endif

