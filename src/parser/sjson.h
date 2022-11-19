#ifndef _headers_1668773822_FluffyLauncher_sjson
#define _headers_1668773822_FluffyLauncher_sjson

//
// Copyright 2018 Sepehr Taghdisian (septag@github). All rights reserved.
// License: https://github.com/septag/sjson#license-bsd-2-clause
//
// Original code by Joseph A. Adams (joeyadams3.14159@gmail.com)
// 
// sjson.h - v1.1.1 - Fast single header json encoder/decoder
//		This is actually a fork of Joseph's awesome Json encoder/decoder code from his repo:
//		https://github.com/rustyrussell/ccan/tree/master/ccan/json
//		The encoder/decoder code is almost the same. What I did was adding object pools and string pages (sjson_context)
//		that eliminates many micro memory allocations, which improves encode/decode speed and data access performance
//		I also added malloc/free and libc API overrides, and made the library single header
//
//	FEATURES:
//		- Single header C-file
//		- UTF8 support
//		- Fast with minimal allocations (Object pool, String pool, ..)
//		- Overriable malloc/free/memcpy/.. 
//	    - Have both Json Decoder/Encoder
//		- Encoder supports pretify
//		- No dependencies
//		- Simple and easy to use C api
//
//	USAGE:
//	Data types:
//		sjson_tag		Json object tag type, string, number, object, etc..
//		sjson_node		Data structure that represents json DOM node
//		sjson_context	Encoder/decoder context, almost all API functions need a context to allocate and manage memory
//						It's not thread-safe, so don't use the same context in multiple threads					
//	API:
//	--- CREATION
//		sjson_create_context	creates a json context:
//								@PARAM pool_size 		number of initial items in object pool (default=512)
//														this is actually the number of DOM nodes in json and can be grown
//														for large files with lots of elements, you can set this paramter to higher values
//								@PARAM str_buffer_size 	String buffer size in bytes (default=4096)
//														Strings in json decoder, encoder uses a special allocator that allocates string from a pool
//														String pool will be grown from the initial size to store more json string data
//														Depending on the size of the json data, you can set this value higher or lower
//								@PARAM alloc_user		A user-defined pointer that is passed to allocations, in case you override memory alloc functions
//	   sjson_destroy_context	Destroys a json context and frees all memory
//								after the context destroys, all created json nodes will become invalid
//	   
//	   sjson_reset_context		Resets the context. No memory will be freed, just resets the buffers so they can be reused
//								but the DOM nodes and strings will be invalidated next time you parse or decode a json
//
//  --- DECODE/ENCODE/VALIDATE
//	   sjson_decode				Decodes the json text and returns DOM document root node
//	   sjson_encode				Encodes the json root node to json string, does not prettify
//							    Generated string can be freed by calling 'sjson_free_string' on the returned pointer
//	   sjson_stringify			Encodes the json root node to pretty json string
//								@PARAM space sets number of spaces for tab, example two-space for a tab is "  "
//	   sjson_free_string		String pointer returned by stringify and encode should be freed by calling this function		
//	   sjson_validate			Validates the json string, returns false if json string is invalid	
//
// --- LOOKUP/TRAVERSE
//	   sjson_find_element		Finds an element by index inside array 		
//	   sjson_find_member		Finds an element by name inside object				
//	   sjson_find_member_nocase	Finds an element by name inside object, ignores case			
//	   sjson_first_child		Gets first child of Object or Array types		
//	   sjson_foreach			Iterates through an Object or array child elements, first parameter should be a pre-defined json_node*
//
// --- HIGHER LEVEL LOOKUP
//	   sjson_get_int			Gets an integer value from a child of the specified parent node, sets to 'default_val' if node is not found
//	   sjson_get_float			Gets a float value from a child of the specified parent node, sets to 'default_val' if node is not found
//	   sjson_get_double			Gets a double value from a child of the specified parent node, sets to 'default_val' if node is not found
//	   sjson_get_string			Gets a string value from a child of the specified parent node, sets to 'default_val' if node is not found
// 	   sjson_get_bool			Gets a boolean value from a child of the specified parent node, sets to 'default_val' if node is not found
//	   sjson_get_floats			Returns float array from a child of the specified parent node, returns false if node is not found or does not have enough elements to fill the variable
//	   sjson_get_ints			Returns integer array from a child of the specified parent node, returns false if node is not found or does not have enough elements to fill the variable
//
// --- CONSTRUCTION
//	   sjson_mknull				Creates a NULL type json node
//	   sjson_mknumber			Creates and sets a number type json node	
//	   sjson_mkstring			Creates and sets a string type json node	
//	   sjson_mkobject			Creates an object type json node		
//	   sjson_mkarray			Creates an array type json node 	
//	   sjson_mkbool				Creates and sets boolean type json node
//	   sjson_append_element		Appends the node to the end of an array 
//	   sjson_prepend_element	Prepends the node to the beginning of an array
//	   sjson_append_member		Appends the node to the members of an object, should provide the key name to the element
//	   sjson_prepend_member		Prepends the node to the members of an object, should provide the key name to the element
//	   sjson_remove_from_parent	Removes the node from it's parrent, if there is any
//     sjson_delete_node		Deletes the node and all of it's children (object/array)
//
// --- HIGHER LEVEL CONSTRUCTION these functions use a combination of above to facilitate some operations
//     sjson_put_obj            Add an object with a key(name) to the child of the parent node
//     sjson_put_array          Add an array with a key(name) to the child of the parent node
//	   sjson_put_int			Add an integer value with a key(name) to the child of the specified parent node
//	   sjson_put_float			Add float value with a key(name) to the child of the specified parent node
// 	   sjson_put_double			Add double value with a key(name) to the child of the specified parent node
//	   sjson_put_bool			Add boolean value with a key(name) to the child of the specified parent node
//	   sjson_put_string			Add string value with a key(name) to the child of the specified parent node
//	   sjson_put_floats			Add float array with a key(name) to the child of the specified parent node
//	   sjson_put_ints			Add integer array with a key(name) to the child of the specified parent node
//	   sjson_put_string			Add string array with a key(name) to the child of the specified parent node
//     
// --- DEBUG
//	   sjson_check				Checks and validates the json DOM node recursively
//								Returns false and writes a report to 'errmsg' parameter if DOM document has any encoding problems
//
// IMPLEMENTATION:
//	
//	   To include and implement sjson in your project, you should define SJSON_IMPLEMENTATION before including sjson.h
//	   You can also override memory allocation or any libc functions that the library uses to your own
//	   Here's a list of stuff that can be overriden:
//		 ALLOCATIONS
//			- sjson_malloc(user, size)		'user' is the pointer that is passed in sjson_create_context
//			- sjson_free(user, ptr)
//			- sjson_realloc(user, ptr, size)
//		 DEBUG/ASSERT
//			- sjson_assert
//		 STRINGS
//			- sjson_stricmp
//			- sjson_strcpy
//			- sjson_strcmp
//          - sjson_snprintf
//		 MEMORY 
//			- sjson_memcpy
//			- sjson_memset
//			- sjson_out_of_memory			happens when sjson cannot allocate memory internally
//											Default behaviour is that it asserts and exits the program
//	   Example:
//			#define SJSON_IMPLEMENTATION
//			#define sjson_malloc(user, size)			MyMalloc(user, size)
//			#define sjson_free(user, ptr)				MyFree(user, ptr)
//			#define sjson_realloc(user, ptr, size)		MyRealloc(user, ptr, size)
//	   		#include "sjson.h"	
//			...
//
// NOTE: on sjson_reset_context
//			what reset_context does is that is resets the internal buffers without freeing them
//			this makes context re-usable for the next json document, so don't have to regrow buffers or create another context
//	   Example:
//			sjson_context* ctx = sjson_create_context(0, 0, NULL);	// initial creation
//			sjson_decode(ctx, json1);		// decode json1
//			...								// do some work on data
//			sjson_reset_context(ctx);		// reset the buffers, make sure you don't need json1 data
//			sjson_decode(ctx, json2);		// decode another json
//			...
//

#ifndef SJSON_H_
#define SJSON_H_

#ifdef _MSC_VER
#   ifndef __cplusplus

#   define false   0
#   define true    1
#   define bool    _Bool

/* For compilers that don't have the builtin _Bool type. */
#       if ((defined(_MSC_VER) && _MSC_VER < 1800) || \
        (defined __GNUC__&& __STDC_VERSION__ < 199901L && __GNUC__ < 3)) && !defined(_lint)
typedef unsigned char _Bool;
#       endif

#   endif /* !__cplusplus */

#define __bool_true_false_are_defined   1
#else
#   include <stdbool.h>
#endif
#include <stddef.h>
#include <stdint.h>

// Json DOM object type
typedef enum sjson_tag {
    SJSON_NULL,
    SJSON_BOOL,
    SJSON_STRING,
    SJSON_NUMBER,
    SJSON_ARRAY,
    SJSON_OBJECT,
} sjson_tag;

// Json DOM node struct
typedef struct sjson_node
{
    struct sjson_node* parent; // only if parent is an object or array (NULL otherwise)
    struct sjson_node* prev;
    struct sjson_node* next;
    
    char* key;                 // only if parent is an object (NULL otherwise). Must be valid UTF-8. 
    
    sjson_tag tag;
    union {
        bool bool_;     // SJSON_BOOL
        char* string_;  // SJSON_STRING: Must be valid UTF-8. 
        double number_; // SJSON_NUMBER
        struct {
            struct sjson_node* head;
            struct sjson_node* tail;
        } children;     // SJSON_ARRAY, SJSON_OBJECT
    };
} sjson_node;

// Json context, handles memory, pools, etc.
// Almost every API needs a valid context to work
// Not multi-threaded. Do not use the same context in multiple threads
typedef struct sjson_context sjson_context;

#ifdef __cplusplus
extern "C" {
#endif

sjson_context* sjson_create_context(int pool_size, int str_buffer_size, void* alloc_user);
void 		   sjson_destroy_context(sjson_context* ctx);
void 		   sjson_reset_context(sjson_context* ctx);

// Encoding, decoding, and validation 
sjson_node* sjson_decode(sjson_context* ctx, const char* json);
char*	    sjson_encode(sjson_context* ctx, const sjson_node* node);
char*	    sjson_encode_string(sjson_context* ctx, const char* str);
char*	    sjson_stringify(sjson_context* ctx, const sjson_node* node, const char* space);
void	    sjson_free_string(sjson_context* ctx, char* str);

bool	    sjson_validate(sjson_context* ctx, const char* json);

// Lookup and traversal
sjson_node* sjson_find_element(sjson_node* array, int index);
sjson_node* sjson_find_member(sjson_node* object, const char* key);
sjson_node* sjson_find_member_nocase(sjson_node* object, const char *name);
sjson_node* sjson_first_child(const sjson_node* node);
int         sjson_child_count(const sjson_node* node);

// Higher level lookup/get functions
int 		sjson_get_int(sjson_node* parent, const char* key, int default_val);
float 		sjson_get_float(sjson_node* parent, const char* key, float default_val);
double 		sjson_get_double(sjson_node* parent, const char* key, double default_val);
const char* sjson_get_string(sjson_node* parent, const char* key, const char* default_val);
bool 		sjson_get_bool(sjson_node* parent, const char* key, bool default_val);
bool		sjson_get_floats(float* out, int count, sjson_node* parent, const char* key);
bool		sjson_get_ints(int* out, int count, sjson_node* parent, const char* key);
bool        sjson_get_uints(uint32_t* out, int count, sjson_node* parent, const char* key);
bool        sjson_get_int16s(int16_t* out, int count, sjson_node* parent, const char* key);
bool        sjson_get_uint16s(uint16_t* out, int count, sjson_node* parent, const char* key);

#define sjson_foreach(i, object_or_array)            \
    for ((i) = sjson_first_child(object_or_array);   \
         (i) != NULL;                                \
         (i) = (i)->next)

// Construction and manipulation 
sjson_node* sjson_mknull(sjson_context* ctx);
sjson_node* sjson_mkbool(sjson_context* ctx, bool b);
sjson_node* sjson_mkstring(sjson_context* ctx, const char *s);
sjson_node* sjson_mknumber(sjson_context* ctx, double n);
sjson_node* sjson_mkarray(sjson_context* ctx);
sjson_node* sjson_mkobject(sjson_context* ctx);

void sjson_append_element(sjson_node* array, sjson_node* element);
void sjson_prepend_element(sjson_node* array, sjson_node* element);
void sjson_append_member(sjson_context* ctx, sjson_node* object, const char* key, sjson_node* value);
void sjson_prepend_member(sjson_context* ctx, sjson_node* object, const char* key, sjson_node* value);

void sjson_remove_from_parent(sjson_node* node);

void sjson_delete_node(sjson_context* ctx, sjson_node* node);

// Higher level construction
sjson_node* sjson_put_obj(sjson_context* ctx, sjson_node* parent, const char* key);
sjson_node* sjson_put_array(sjson_context* ctx, sjson_node* parent, const char* key);
sjson_node* sjson_put_int(sjson_context* ctx, sjson_node* parent, const char* key, int val);
sjson_node* sjson_put_float(sjson_context* ctx, sjson_node* parent, const char* key, float val);
sjson_node* sjson_put_double(sjson_context* ctx, sjson_node* parent, const char* key, double val);
sjson_node* sjson_put_bool(sjson_context* ctx, sjson_node* parent, const char* key, bool val);
sjson_node* sjson_put_string(sjson_context* ctx, sjson_node* parent, const char* key, const char* val);
sjson_node* sjson_put_floats(sjson_context* ctx, sjson_node* parent, const char* key, const float* vals, int count);
sjson_node* sjson_put_ints(sjson_context* ctx, sjson_node* parent, const char* key, const int* vals, int count);
sjson_node* sjson_put_strings(sjson_context* ctx, sjson_node* parent, const char* key, const char** vals, int count);
sjson_node* sjson_put_uints(sjson_context* ctx, sjson_node* parent, const char* key, const uint32_t* vals, int count);
sjson_node* sjson_put_int16s(sjson_context* ctx, sjson_node* parent, const char* key, const int16_t* vals, int count);
sjson_node* sjson_put_uint16s(sjson_context* ctx, sjson_node* parent, const char* key, const uint16_t* vals, int count);

// Debugging

/*
 * Look for structure and encoding problems in a sjson_node or its descendents.
 *
 * If a problem is detected, return false, writing a description of the problem
 * to errmsg (unless errmsg is NULL).
 */
bool sjson_check(const sjson_node* node, char errmsg[256]);

#ifdef __cplusplus
}
#endif

#endif // SJSON_H_

//
// Version History:
//			1.0.0			Initial release
//			1.1.0			Added higher level json get/put functions
//							Implemented sjson_reset_context
//          1.1.1           Fixed some macro defines (sjson_strcpy, sjson_snprintf)
//

#endif

