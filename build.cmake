# Build config

set(BUILD_PROJECT_NAME "FluffyMCLauncher")

# We're making library
set(BUILD_IS_LIBRARY NO)
set(BUILD_IS_KERNEL NO)

# If we want make libary and
# executable project
set(BUILD_INSTALL_EXECUTABLE YES)
set(BUILD_MAXIMUM_PERFORMANCE NO)

# Sources which common between exe and library
set(BUILD_SOURCES
  src/auth/microsoft_auth.c
  src/auth/microsoft_auth/stage1.c
  src/auth/microsoft_auth/stage2.c
  src/auth/xbox_live_auth.c
  src/auth/minecraft_auth.c
  src/auth/xsts_auth.c
  src/auth/xbl_like_auth.c
  
  src/minecraft_api/api.c
  src/minecraft_api/schema.c
  src/stacktrace/stacktrace.c
  src/stacktrace/provider/libbacktrace.c
  
  src/networking/http_request.c
  src/networking/http_response.c
  src/networking/http_headers.c
  src/networking/http_headers_serializer/normal.c
  src/networking/transport/transport_socket.c
  src/networking/transport/transport_ssl.c
  src/networking/transport/transport.c
  src/networking/networking.c
  src/networking/easy.c
 
  src/util/circular_buffer.c
  src/util/util.c
  src/util/uwuify.c
  src/util/json_schema_loader.c
  
  src/logging/logging.c
  src/io/io_threads.c
  
  src/parser/json/json.c
  src/parser/json/decoder/builtin.c
  src/parser/json/decoder/cjson.c
  src/parser/json/decoder.c
  
  src/dummy.c
  src/panic.c
  
  src/submodules/cJSON/cJSON.c
  
  deps/vec/vec.c
  deps/buffer/buffer.c
  deps/templated-hashmap/hashmap.c
  deps/list/list.c
  deps/list/list_node.c
  deps/list/list_iterator.c
)

set(BUILD_INCLUDE_DIRS
  deps/vec/ 
  deps/buffer/ 
  deps/templated-hashmap/ 
  deps/list/  
)

# Note that exe does not represent Windows' 
# exe its just short hand of executable 
#
# Note:
# Still creates executable even building library. 
# This would contain test codes if project is 
# library. The executable directly links to the 
# library objects instead through shared library
set(BUILD_EXE_SOURCES
  src/specials.c
  src/premain.c
  src/main.c
)

# Public header to be exported
# If this a library
set(BUILD_PUBLIC_HEADERS
  include/dummy.h
)

set(BUILD_PROTOBUF_FILES
)

set(BUILD_CFLAGS "-gdwarf-4")
set(BUILD_LDFLAGS "-gdwarf-4")

# AddPkgConfigLib is in ./buildsystem/CMakeLists.txt
macro(AddDependencies)
  # Example
  # AddPkgConfigLib(FluffyGC FluffyGC>=1.0.0)
  
  link_libraries(-lcrypto -lssl)
  
  if (DEFINED CONFIG_STACKTRACE_USE_DLADDR)
    link_libraries(-ldl)
  endif()
  
  if (DEFINED CONFIG_STACKTRACE_PROVIDER_LIBBACKTRACE)
    link_libraries(-lbacktrace)
  endif()
endmacro()


