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
  
  src/logging/logging.c
  src/parser/sjson.c
  src/io/io_threads.c
  
  src/dummy.c

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

set(BUILD_CFLAGS "")
set(BUILD_LDFLAGS "")

# AddPkgConfigLib is in ./buildsystem/CMakeLists.txt
macro(AddDependencies)
  # Example
  # AddPkgConfigLib(FluffyGC FluffyGC>=1.0.0)
  
  link_libraries(-lcrypto -lssl)
endmacro()


