#include "decoder.h"
#include "config.h"
#include "decoder/builtin.h"
#include <stdio.h>

#if IS_ENABLED(CONFIG_JSON_DECODER_DEFAULT_DAVEGAMBLE_CJSON)
# include "decoder/cjson.h"
#endif

int json_decode_default(struct json_node** root, const char* data, size_t len) { 
# if IS_ENABLED(CONFIG_JSON_DECODER_DEFAULT_DAVEGAMBLE_CJSON)
  return json_decode_cjson(root, data, len);
# elif IS_ENABLED(CONFIG_JSON_DECODER_DEFAULT_BUILTIN)
  return json_decode_builtin(root, data, len);
# endif
}
