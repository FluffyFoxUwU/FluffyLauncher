#include <errno.h>
#include <inttypes.h>
#include <stdlib.h>
#include <limits.h>

#include "http_response.h"
#include "http_headers.h"
#include "bug.h"
#include "util/util.h"
#include "http_request.h"
#include "transport/transport.h"

static int initResponse(struct http_response* self, bool isStatic) {
  int res = 0;
  *self = (struct http_response) {
    .staticlyAllocated = isStatic
  };
  
  self->headers = http_headers_new();
  if (!self->headers) {
    res = -ENOMEM;
    goto failure;
  }
 
failure:
  return res; 
}

struct http_response* http_response_new() {
  struct http_response* self = malloc(sizeof(*self));
  int res = initResponse(self, false);
  
  if (res < 0) {
    http_response_free(self);
    return NULL;
  }
  return self;
}

int http_response_static_init(struct http_response* self) {
  return initResponse(self, true);
}

void http_response_free(struct http_response* self) {
  if (!self)
    return;
  
  free((char*) self->description);
  http_headers_free(self->headers);
  
  self->description = NULL;
  self->headers = NULL;
  
  if (self->staticlyAllocated)
    return;
  free(self);
}

// Read one line terminated by \r\n
// Return 0 on success
static int readOneLine(struct transport* transport, buffer_t* lineBuffer) {
  char currentLine = '\0';
  int res = 0;
  bool isPreviousCarriage = false;
  while ((res = transport->read(transport, &currentLine, 1, NULL)) >= 0) {
    if (buffer_append_n(lineBuffer, &currentLine, 1) < 0)
      return -ENOMEM;
    
    if (currentLine == '\r') {
      isPreviousCarriage = true;
      continue;
    }
    
    // EOL found
    if (currentLine == '\n' && isPreviousCarriage) {
      lineBuffer->data[buffer_length(lineBuffer) - 2] = '\0';
      break;
    }
    
    isPreviousCarriage = false;
  }
  
  return res;
}

static int readHeaders(struct http_response* self, struct transport* transport) {
  int res = 0;
  buffer_t* line = buffer_new();
  if (!line)
    return -ENOMEM;
  
  do {
    buffer_clear(line);
    if (readOneLine(transport, line) < 0) {
      res = -EFAULT;
      goto malformed_response;
    }

    // End of headers list (the \r\n end is stripped of by readOneLine)
    if (buffer_length(line) == 0)
      break;

    // Add it to response header list
    char* current = line->data;
    while (*current != ':' && *current != '\0')
      current++;
    
    if (*current != ':') {
      res = -EFAULT;
      goto malformed_response;
    }
    *current = '\0';
    
    const char* name = line->data;
    const char* value = current + 1; 
    if ((res = http_headers_add(self->headers, name, value)) < 0) {
      if (res != -ENOMEM)
        res = -EFAULT;
      goto malformed_response;
    }
  } while (buffer_length(line) > 0);

malformed_response:
  buffer_free(line);
  return 0;
}

static int readStatusLine(struct http_response* self, struct transport* transport) {
  int res = 0;
  buffer_t* line = buffer_new();
  if (!line)
    return -ENOMEM;
  
  if (readOneLine(transport, line) < 0) {
    res = -EFAULT;
    goto malformed_response;
  }
  
  // Parse response
  const char* delim = " ";
  char* current = NULL;
  char* protocol = strtok_r(buffer_string(line), delim, &current);
  char* statusString = strtok_r(NULL, delim, &current);
  char* description = current;
  
  // Malformed response
  if (*current == '\0') {
    res = -EFAULT;
    goto malformed_response;
  }
  
  errno = 0;
  uintmax_t status = strtoumax(statusString, &current, 10);
  if (errno != 0 || current == statusString || *current != '\0' ||
      status > INT_MAX) {
    res = -EFAULT;
    goto malformed_response;
  }
  
  // We expect server to return same major version in response but server
  // responsed in other HTTP major version which is unacceptable
  if (strncmp(protocol, HTTP_PROTOCOL_VERSION_MAJOR_ONLY, sizeof(HTTP_PROTOCOL_VERSION_MAJOR_ONLY) - 1) != 0) {
    res = -EFAULT;
    goto malformed_response;
  }
  
  self->description = strdup(description);
  self->status = status;
  
malformed_response:
  buffer_free(line);
  return res;
}

enum transfer_method {
  HTTP_TRANSFER_UNKNOWN,
  
  // Chunked transfer mode
  HTTP_TRANSFER_CHUNKED,
  
  // Transfer by content length
  HTTP_TRANSFER_BY_CONTENT_LENGTH,
  
  // Transfer until connection closed
  HTTP_TRANSFER_UNTIL_CLOSED,
};

// Contain information about transfer method
struct transfer_method_data {
  struct http_response* response; 
  FILE* writeTarget;
  
  union {
    struct {
      size_t length;
    } byContentLength;
  } data;
};

// Determine what transfer method the server uses
static enum transfer_method determineTransferMethod(struct http_response* self, struct transfer_method_data* transferMethodData) {
  // Check for Transfer-Encoding 
  vec_str_t* transferEncoding = http_headers_get(self->headers, "Transfer-Encoding");
  if (transferEncoding) {
    const char* method = vec_last(transferEncoding);
    
    if (strstr(method, "chunked") != NULL) 
      return HTTP_TRANSFER_CHUNKED;
    else if (strstr(method, "identity") != NULL) 
      goto identity_encoding; /* Proceed to next check */
    
    // We cant passthrough as we dont know how to read the data
    goto unknown_transfer_encoding;
  }

identity_encoding:;
  // Check for Content-Length
  vec_str_t* contentLength = http_headers_get(self->headers, "Content-Length");
  if (contentLength) {
    size_t length = 0;
    const char* content = vec_last(contentLength);
    while (*content == ' ' && *content != '\0')
      content++;
    if (*content == '\0')
      goto invalid_content_length_header;
    
    errno = 0;
    length = strtoumax(content, NULL, 10);
    if (errno)
      goto invalid_content_length_header;
    
    transferMethodData->data.byContentLength.length = length;
    return HTTP_TRANSFER_BY_CONTENT_LENGTH;
  }

  // Check for Connection
  vec_str_t* connection = http_headers_get(self->headers, "Connection");
  if (connection) {
    const char* connectionType = vec_last(connection);
    
    if (strstr(connectionType, "close") != NULL) {
      return HTTP_TRANSFER_UNTIL_CLOSED;
    }
    
    // We can passthrough and continue to check for other methods  
  }

invalid_content_length_header:
unknown_transfer_encoding:
  return HTTP_TRANSFER_UNKNOWN;
}

static bool isHex(char chr) {
  static bool lookup[256] = {
    ['0'] = true, ['1'] = true, ['2'] = true, ['3'] = true, ['4'] = true, ['5'] = true, ['6'] = true, ['7'] = true,
    ['8'] = true, ['9'] = true, ['a'] = true, ['b'] = true, ['c'] = true, ['d'] = true, ['e'] = true, ['f'] = true,
    ['A'] = true, ['B'] = true, ['C'] = true, ['D'] = true, ['E'] = true, ['F'] = true
  };
  
  return lookup[(int) chr];
} 

static int bodyReceived(struct transfer_method_data* transferMethodData, const void* data, size_t len) {
  fwrite(data, 1, len, transferMethodData->writeTarget);
  if (ferror(transferMethodData->writeTarget) != 0)
    return -EIO;
  transferMethodData->response->writtenSize += len; 
  return 0;
}

// TODO: Implement chunk-extension as defined by https://www.rfc-editor.org/rfc/rfc9112.html#section-7.1
static int readChunkedMode(struct http_response* self, struct transport* transport, struct transfer_method_data* transferMethodData) {
  int res = 0;
  buffer_t* line = buffer_new();
  if (!line)
    return -ENOMEM;
  
  size_t chunkSize = 0;
  do {
    if ((res = readOneLine(transport, line)) < 0)
      goto fail_line_read;
    
    // Found great trick by just check for 0x and non hex then bail out instead
    if (isHex(buffer_string(line)[0]) == false || 
        strncmp(buffer_string(line), "0x", 2) == 0) {
      res = -EFAULT;
      goto malformed_chunked;
    }
    
    errno = 0;
    uintmax_t tmp = strtoumax(buffer_string(line), NULL, 16);
    if (errno || tmp > SIZE_MAX) {
      res = -EFAULT;
      goto malformed_chunked;
    }
    chunkSize = (size_t) tmp;
    
    if (chunkSize == 0)
      goto skip_read;
    
    // Read data here
    size_t readSize = 0;
    char* buffer = malloc(chunkSize);
    
    res = transport->read(transport, buffer, chunkSize, &readSize);
    if (res < 0)
      goto transport_error;
      
    res = bodyReceived(transferMethodData, buffer, chunkSize);
    free(buffer);
    if (res < 0)
      goto io_error;
    
    buffer_clear(line);
    
    // Check for last \r\n sequence 
    if ((res = readOneLine(transport, line)) < 0)
      goto fail_line_read;
    
    if (buffer_length(line) != 0) {
      res = -EFAULT;
      goto malformed_chunked;
    }
skip_read:
    buffer_clear(line);
  } while (chunkSize != 0);

io_error:
transport_error:
malformed_chunked:
fail_line_read:  
  buffer_free(line);
  return res;
}

static int readByLengthMode(struct http_response* self, struct transport* transport, struct transfer_method_data* transferMethodData) {
  int res = 0;
  char buffer[4096] = {};
  size_t initialReadSize = transferMethodData->data.byContentLength.length % sizeof(buffer);
  size_t readSize = 0;
  if ((res = transport->read(transport, buffer, initialReadSize, &readSize)) < 0)
    goto transport_error;
  if ((res = bodyReceived(transferMethodData, buffer, readSize)) < 0)
    goto io_error;
  
  // Number of full buffers to read
  size_t bufferCount = (transferMethodData->data.byContentLength.length - initialReadSize) / sizeof(buffer);
  for (size_t i = 0; i < bufferCount; i++) {
    res = transport->read(transport, buffer, sizeof(buffer), &readSize);
    if (fwrite(buffer, 1, readSize, transferMethodData->writeTarget) == 0)
      return -EIO;
    self->writtenSize += readSize;
    
    if (res < 0)
      goto transport_error;
  }

io_error:
transport_error:
  BUG_ON(res == -ENODATA);
  return res;
}

static int readUntilClosed(struct http_response* self, struct transport* transport, struct transfer_method_data* transferMethodData) {
  int res = 0;
  char buffer[4096] = {};
  
  size_t sizeRead = 0;
  while ((res = transport->read(transport, buffer, sizeof(buffer), &sizeRead)) >= 0)
    if ((res = bodyReceived(transferMethodData, buffer, sizeRead)) < 0)
      goto io_error;
  
  if (res == -ENODATA) {
    if ((res = bodyReceived(transferMethodData, buffer, sizeRead)) < 0)
      goto io_error;
    res = 0;
  }

io_error:
  return res;
}

int http_response_recv(struct http_response* _self, struct transport* transport, FILE* writeTo) {
  int res = 0;
  
  struct http_response self = {};
  if ((res = http_response_static_init(&self)) < 0)
    goto error_init_self;
  
  // Reading response
  if ((res = readStatusLine(&self, transport)) < 0)
    goto read_response_failure;
  if ((res = readHeaders(&self, transport)) < 0)
    goto read_response_failure;
  
  struct transfer_method_data transferMethodData = {
    .writeTarget = writeTo,
    .response = &self
  };
  enum transfer_method transferMethod = determineTransferMethod(&self, &transferMethodData);
  switch (transferMethod) {
    case HTTP_TRANSFER_CHUNKED:
      res = readChunkedMode(&self, transport, &transferMethodData);
      break;
    case HTTP_TRANSFER_BY_CONTENT_LENGTH:
      res = readByLengthMode(&self, transport, &transferMethodData);
      break;
    case HTTP_TRANSFER_UNTIL_CLOSED:
      res = readUntilClosed(&self, transport, &transferMethodData);
      break;
    case HTTP_TRANSFER_UNKNOWN:
      res = -ENOTSUP;
      goto unknown_transfer_method;
    default:
      BUG();
  }
  
  if (res < 0)
    goto transfer_error;

  res = self.status;
  if (_self) {
    self.staticlyAllocated = false;
    *_self = self;
  } else { 
    http_response_free(&self);
  }

transfer_error: 
unknown_transfer_method:
read_response_failure: 
error_init_self:
  return res;
}
