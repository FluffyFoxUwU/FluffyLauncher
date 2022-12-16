#include <limits.h>
#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <threads.h>

#include "buffer.h"
#include "parser/json/json.h"
#include "builtin.h"
#include "config.h"
#include "bug.h"

struct decoder_entry;
struct decoder_context;

struct decoder_entry {
  int (*decodeFunc)(struct decoder_context*, struct decoder_entry*);
  struct json_node* node;
};

struct decoder_context {
  const char* data;
  size_t size;
  int offset;
  
  int sp;
  struct decoder_entry stack[CONFIG_JSON_NEST_MAX];
};

static int stackPush(struct decoder_context* ctx, struct decoder_entry entry) {
  if (ctx->sp >= CONFIG_JSON_NEST_MAX)
    return -EOVERFLOW;
  
  ctx->stack[ctx->sp] = entry;
  ctx->sp++;
  return 0;
}

static void stackPop(struct decoder_context* ctx, struct decoder_entry* entry) {
  BUG_ON(ctx->sp <= 0);
  *entry = ctx->stack[ctx->sp - 1];
  ctx->sp--;
}

#define peek(ctx) (ctx->data[ctx->offset])

// Return EOF 
static int getChar(struct decoder_context* ctx) {
  if (ctx->offset >= ctx->size)
    return EOF;
  return ctx->data[ctx->offset++];
}

static int skipWhitespace(struct decoder_context* ctx) {
  static bool isWhite[256] = {
    [' '] = true,
    ['\n'] = true,
    ['\r'] = true,
    ['\t'] = true
  };
  
  int res;
  while ((res = getChar(ctx)))
    if (!isWhite[res] || res == EOF)
      return -ENODATA;
  
  return 0;
}

static int parseObject(struct decoder_context* ctx, struct decoder_entry* entry) {
  BUG_ON(peek(ctx) != '{');
  int res = 0;
  
  return res;
}

int json_decode_builtin(struct json_node** root, const char* data, size_t len) {
  // TODO: Built in decoder not ready yet
  BUG();
  
  (void) parseObject;
  (void) skipWhitespace;
  (void) stackPush;
  (void) stackPop;
  
  if (len > INT_MAX)
    return -E2BIG;
  
  int res = 0;
  
  static thread_local struct decoder_context ctx = {
    .stack = {}
  };
  ctx.data = data;
  ctx.offset = 0;
  ctx.sp = 0;
  ctx.size = len;
  
  if (res < 0)
    json_free(ctx.stack[0].node);
  
  else if (root)
    *root = ctx.stack[0].node;
  return res;
}
