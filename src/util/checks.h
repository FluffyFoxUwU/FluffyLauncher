#ifndef _headers_1671335743_FluffyLauncher_checks
#define _headers_1671335743_FluffyLauncher_checks

#include <assert.h>

#include "util.h"

#define __check_struct_field(id, t, f, ...) typedef __VA_ARGS__ struct_check_type_ ## id; [[maybe_unused]] static t* struct_check_ ## id ; static_assert(is_same_type(struct_check_ ## id ->f, struct_check_type_ ## id))
#define _check_struct_field(id, t, f, ...) __check_struct_field(id, t, f, __VA_ARGS__)
#define check_struct_field(t, f, ...) _check_struct_field(__LINE__, t, f, __VA_ARGS__)

#endif

