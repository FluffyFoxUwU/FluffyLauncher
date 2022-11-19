#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <limits.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <inttypes.h>

#include "bug.h"
#include "buffer.h"
#include "http_headers.h"
#include "transport/transport.h"
#include "http_request.h"
#include "hashmap.h"
#include "util.h"
#include "vec.h"

// TODO: Split this file into smaller chunks
// which does specific part of HTTP request like
// a seperate file just containing function to
// generate request payload and another for 
// parsing response

struct http_request* http_new_request() {
  struct http_request* self = malloc(sizeof(*self));
  if (!self)
    return NULL;
  
  *self = (struct http_request) {};
  self->headers = http_headers_new();
  self->isMethodSet = false;
  self->canFreeLocation = false;
  if (!self->headers)
    goto headers_alloc_fail;
  
  return self;

headers_alloc_fail:
  http_free_request(self);
  return NULL;
}

void http_free_request(struct http_request* self) {
  if (!self)
    return;
  
  http_headers_free(self->headers);
  if (self->canFreeLocation)
    free((char*) self->location);
  free(self);
}

void http_free_response(struct http_response* self) {
  if (!self)
    return;
  
  free((char*) self->description);
  http_headers_free(self->headers);
  free(self);
}

int http_set_location_formatted(struct http_request* self, const char* urlFmt, ...) {
  va_list list;
  va_start(list, urlFmt);
  int res = http_set_location_formatted_va(self, urlFmt, list);
  va_end(list);
  return res;
}

void http_set_location(struct http_request* self, const char* location) {
  if (self->canFreeLocation)
    free((char*) self->location);
  
  self->location = location;
  self->canFreeLocation = false;
}

static bool isValidLocation(const char* location) {
  // https://0mg.github.io/tools/uri/
  static char lookup[256] = {
    ['t'] = true,  ['h'] = true,  ['e'] = true,  ['l'] = true,  ['a'] = true,  ['z'] = true,  ['y'] = true,  ['q'] = true, 
    ['u'] = true,  ['i'] = true,  ['c'] = true,  ['k'] = true,  ['f'] = true,  ['o'] = true,  ['x'] = true,  ['j'] = true, 
    ['m'] = true,  ['p'] = true,  ['s'] = true,  ['v'] = true,  ['r'] = true,  ['b'] = true,  ['w'] = true,  ['n'] = true, 
    ['d'] = true,  ['g'] = true,  ['T'] = true,  ['H'] = true,  ['E'] = true,  ['L'] = true,  ['A'] = true,  ['Z'] = true, 
    ['Y'] = true,  ['Q'] = true,  ['U'] = true,  ['I'] = true,  ['C'] = true,  ['K'] = true,  ['F'] = true,  ['O'] = true, 
    ['X'] = true,  ['J'] = true,  ['M'] = true,  ['P'] = true,  ['S'] = true,  ['V'] = true,  ['R'] = true,  ['B'] = true, 
    ['W'] = true,  ['N'] = true,  ['D'] = true,  ['G'] = true,  ['0'] = true,  ['1'] = true,  ['2'] = true,  ['3'] = true, 
    ['4'] = true,  ['5'] = true,  ['6'] = true,  ['7'] = true,  ['8'] = true,  ['9'] = true,  ['-'] = true,  ['.'] = true, 
    ['_'] = true,  ['~'] = true,  ['!'] = true,  ['$'] = true,  ['&'] = true,  ['('] = true,  [')'] = true,  ['*'] = true, 
    ['+'] = true,  [','] = true,  [';'] = true,  ['='] = true,  [':'] = true,  ['@'] = true,  ['/'] = true,  ['?'] = true,
    ['\''] = true
  };
  
  // Must be absolute path
  if (location[0] != '/')
    return -EINVAL;
  
  while (*location != '\0') { 
    if (!lookup[(int) *location])
      return false;
    
    location++;
  }
  
  return true;
}

int http_set_location_formatted_va(struct http_request* self, const char* urlFmt, va_list list) {
  util_asprintf((char**) &self->location, urlFmt, list);
  if (!self->location)
    return -ENOMEM;
  
  if (!isValidLocation(self->location)) {
    free((char*) self->location);
    self->location = NULL;
    return -EINVAL;
  }
  
  self->canFreeLocation = true;
  return 0;
}

void http_set_method(struct http_request* self, enum http_method method) {
  self->method = method;
  self->isMethodSet = true; 
}

static const char* methodAsString(enum http_method method) {
  switch (method) {
    case HTTP_GET:
      return "GET";
    case HTTP_POST:
      return "POST";
    case HTTP_DELETE:
      return "DELETE";
    case HTTP_PATCH:
      return "PATCH";
    case HTTP_PUT:
      return "PUT";
  }
  
  BUG();
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

static int readHeaders(struct http_response* response, struct transport* transport) {
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
    if ((res = http_headers_add(response->headers, name, value)) < 0) {
      if (res != -ENOMEM)
        res = -EFAULT;
      goto malformed_response;
    }
  } while (buffer_length(line) > 0);

malformed_response:
  buffer_free(line);
  return 0;
}

static int sendRequest(struct http_request* self, struct transport* transport, const char* httpVer) {
  int res = 0;
  buffer_t* requestPayload = buffer_new();
  if (!requestPayload) {
    res = -ENOMEM;
    goto request_buffer_alloc_failure;
  }
  
  buffer_t* requestHeader = http_headers_serialize(self->headers, HTTP_HEADER_NORMAL);
  if (!requestHeader) {
    res = -ENOMEM;
    goto serialize_request_header_failure;
  }
  
  res = buffer_appendf(requestPayload, "%s %s %s\r\n%s\r\n", methodAsString(self->method), self->location, httpVer, buffer_string(requestHeader));
  buffer_free(requestHeader);
  if (res < 0) {
    res = -ENOMEM;
    goto payload_creation_failure;
  }
  
  // Sending request
  res = transport->write(transport, buffer_string(requestPayload), buffer_length(requestPayload));
  buffer_free(requestPayload);
  if (res < 0) 
    goto write_request_payload_failure;
  
  // Sending request body if exist
  if (self->requestData) 
    if ((res = transport->write(transport, self->requestData, self->requestDataLen)) < 0) 
      goto write_request_payload_failure; 
write_request_payload_failure:
payload_creation_failure:
serialize_request_header_failure:
request_buffer_alloc_failure:
  return res;
}

static int readStatusLine(struct http_response* response, struct transport* transport) {
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
  
  response->description = strdup(description);
  response->status = status;
  
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
  FILE* writeTarget;
  
  union {
    struct {
      size_t length;
    } byContentLength;
  } data;
};

// Determine what transfer method the server uses
static enum transfer_method determineTransferMethod(struct http_response* response, struct transfer_method_data* transferMethodData) {
  // Check for Transfer-Encoding 
  vec_str_t* transferEncoding = http_headers_get(response->headers, "Transfer-Encoding");
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
  vec_str_t* contentLength = http_headers_get(response->headers, "Content-Length");
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
  vec_str_t* connection = http_headers_get(response->headers, "Connection");
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

// TODO: Implement chunk-extension as defined by https://www.rfc-editor.org/rfc/rfc9112.html#section-7.1
static int readChunkedMode(struct http_response* response, struct transport* transport, struct transfer_method_data* transferMethodData) {
  int res = 0;
  buffer_t* line = buffer_new();
  if (!line)
    return -ENOMEM;
  
  puts("Chunked mode!");
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
    
    printf("Size: %zu\n", chunkSize);
    if (chunkSize == 0)
      goto skip_read;
    
    // Read data here
    size_t readSize = 0;
    char* buffer = malloc(chunkSize);
    
    res = transport->read(transport, buffer, chunkSize, &readSize);
    if (fwrite(buffer, 1, readSize, transferMethodData->writeTarget) == 0) {
      free(buffer);
      return -EIO;
    }
    free(buffer);
    response->writtenSize += readSize;
    
    if (res < 0)
      goto transport_error;
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

transport_error:
malformed_chunked:
fail_line_read:  
  buffer_free(line);
  return res;
}

static int readByLengthMode(struct http_response* response, struct transport* transport, struct transfer_method_data* transferMethodData) {
  int res = 0;
  char buffer[4096] = {};
  size_t initialReadSize = transferMethodData->data.byContentLength.length % sizeof(buffer);
  size_t readSize = 0;
  res = transport->read(transport, buffer, initialReadSize, &readSize);
  if (fwrite(buffer, 1, readSize, transferMethodData->writeTarget) == 0)
    return -EIO;
  response->writtenSize += readSize;
  
  if (res < 0)
    goto transport_error;
  
  // Number of full buffers to read
  size_t bufferCount = (transferMethodData->data.byContentLength.length - initialReadSize) / sizeof(buffer);
  for (size_t i = 0; i < bufferCount; i++) {
    res = transport->read(transport, buffer, sizeof(buffer), &readSize);
    if (fwrite(buffer, 1, readSize, transferMethodData->writeTarget) == 0)
      return -EIO;
    response->writtenSize += readSize;
    
    if (res < 0)
      goto transport_error;
  }

transport_error:
  if (res == -ENODATA)
    res = 0;
  return res;
}

static int readUntilClosed(struct http_response* response, struct transport* transport, struct transfer_method_data* transferMethodData) {
  int res = 0;
  char buffer[4096] = {};
  
  size_t sizeRead = 0;
  while ((res = transport->read(transport, buffer, sizeof(buffer), &sizeRead)) >= 0) {
    if (fwrite(buffer, 1, sizeRead, transferMethodData->writeTarget) == 0)
      return -EIO;
    response->writtenSize += sizeRead;
  }
  
  // Write the rest if something left when error occured
  if (res < 0 && fwrite(buffer, 1, sizeRead, transferMethodData->writeTarget) == 0) {
    response->writtenSize += sizeRead;
    return -EIO;
  }
  
  if (res == -ENODATA)
    res = 0;
  
  return res;
}

int http_exec(struct http_request* self, struct transport* transport, struct http_response** responsePtr, FILE* writeTo) {
  if (self->location == NULL || !self->isMethodSet) 
    return -EINVAL;
  
  int res = 0;
  struct http_response* response = malloc(sizeof(*response));
  if (!response) {
    res = -ENOMEM;
    goto response_alloc_failure;
  }
  *response = (struct http_response) {};
  
  response->headers = http_headers_new();
  if (!response->headers) {
    res = -ENOMEM;
    goto headers_alloc_failure;  
  }
  
  // Sending request
  if ((res = sendRequest(self, transport, HTTP_PROTOCOL_VERSION)) < 0)
    goto send_request_failure;
  
  // Reading response
  if ((res = readStatusLine(response, transport)) < 0)
    goto read_response_failure;
  if ((res = readHeaders(response, transport)) < 0)
    goto read_response_failure;
  
  struct transfer_method_data transferMethodData = {
    .writeTarget = writeTo
  };
  enum transfer_method transferMethod = determineTransferMethod(response, &transferMethodData);
  switch (transferMethod) {
    case HTTP_TRANSFER_CHUNKED:
      res = readChunkedMode(response, transport, &transferMethodData);
      break;
    case HTTP_TRANSFER_BY_CONTENT_LENGTH:
      res = readByLengthMode(response, transport, &transferMethodData);
      break;
    case HTTP_TRANSFER_UNTIL_CLOSED:
      res = readUntilClosed(response, transport, &transferMethodData);
      break;
    case HTTP_TRANSFER_UNKNOWN:
      res = -ENOTSUP;
      goto unknown_transfer_method;
    default:
      BUG();
  }
  
  if (res < 0)
    goto transfer_error;

  res = response->status;

transfer_error: 
unknown_transfer_method:
read_response_failure: 
send_request_failure:
headers_alloc_failure:
  if (res < 0 || responsePtr == NULL)
    http_free_response(response);
response_alloc_failure:
  if (responsePtr && res >= 0) 
    *responsePtr = response;
  
  return res;
}




