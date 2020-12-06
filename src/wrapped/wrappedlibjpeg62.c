#define _GNU_SOURCE         /* See feature_test_macros(7) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
#include <setjmp.h>

#include "wrappedlibs.h"

#include "debug.h"
#include "wrapper.h"
#include "bridge.h"
#include "librarian/library_private.h"
#include "x86emu.h"
#include "emu/x86emu_private.h"
#include "callback.h"
#include "librarian.h"
#include "box86context.h"
#include "emu/x86emu_private.h"
#include "myalign.h"

const char* libjpeg62Name = "libjpeg.so.62";
#define LIBNAME libjpeg62
#define ALTNAME "libjpeg.so.8"

static library_t* my_lib = NULL;
static bridge_t* my_bridge = NULL;

typedef void    (*vFp_t)    (void*);
typedef void*   (*pFp_t)    (void*);
typedef int     (*iFp_t)    (void*);
typedef int     (*iFpi_t)   (void*, int);
typedef void    (*vFpip_t)  (void*, int, void*);
typedef void    (*vFpiL_t)  (void*, int, unsigned long);
typedef uint32_t(*uFppu_t)  (void*, void*, uint32_t);

#define SUPER() \
    GO(jpeg_CreateDecompress, vFpiL_t)  \
    GO(jpeg_read_header, iFpi_t)        \
    GO(jpeg_start_decompress, iFp_t)    \
    GO(jpeg_read_scanlines, uFppu_t)    \
    GO(jpeg_finish_decompress, iFp_t)   \
    GO(jpeg_std_error, pFp_t)           \
    GO(jpeg_set_marker_processor, vFpip_t)  \
    GO(jpeg_destroy_decompress, vFp_t)  \

typedef struct jpeg62_my_s {
    // functions
    #define GO(A, B)    B   A;
    SUPER()
    #undef GO
} jpeg62_my_t;

void* getJpeg62My(library_t* lib)
{
    jpeg62_my_t* my = (jpeg62_my_t*)calloc(1, sizeof(jpeg62_my_t));
    #define GO(A, W) my->A = (W)dlsym(lib->priv.w.lib, #A);
    SUPER()
    #undef GO
    return my;
}
#undef SUPER

void freeJpeg62My(void* lib)
{
    //jpeg62_my_t *my = (jpeg62_my_t *)lib;
}

typedef struct jpeg62_error_mgr_s {
  void (*error_exit) (void* cinfo);
  void (*emit_message) (void* cinfo, int msg_level);
  void (*output_message) (void* cinfo);
  void (*format_message) (void* cinfo, char *buffer);
  void (*reset_error_mgr) (void* cinfo);
  int msg_code;
  union {
    int i[8];
    char s[80];
  } msg_parm;
  int trace_level;
  long num_warnings;
  const char * const *jpeg_message_table;
  int last_jpeg_message;
  const char * const *addon_message_table;
  int first_addon_message;
  int last_addon_message;
} jpeg62_error_mgr_t;

typedef struct jpeg62_memory_mgr_s {
  void* (*alloc_small) (void* cinfo, int pool_id, size_t sizeofobject);
  void* (*alloc_large) (void* cinfo, int pool_id, size_t sizeofobject);
  void* (*alloc_sarray) (void* cinfo, int pool_id, uint32_t samplesperrow, uint32_t numrows);
  void* (*alloc_barray) (void* cinfo, int pool_id, uint32_t blocksperrow, uint32_t numrows);
  void* (*request_virt_sarray) (void* cinfo, int pool_id,
                                           int pre_zero,
                                           uint32_t samplesperrow,
                                           uint32_t numrows,
                                           uint32_t maxaccess);
  void* (*request_virt_barray) (void* cinfo, int pool_id,
                                           int pre_zero,
                                           uint32_t blocksperrow,
                                           uint32_t numrows,
                                           uint32_t maxaccess);
  void (*realize_virt_arrays) (void* cinfo);
  void* (*access_virt_sarray) (void* cinfo, void* ptr,
                                    uint32_t start_row, uint32_t num_rows,
                                    int writable);
  void* (*access_virt_barray) (void* cinfo, void* ptr,
                                     uint32_t start_row, uint32_t num_rows,
                                     int writable);
  void (*free_pool) (void* cinfo, int pool_id);
  void (*self_destruct) (void* cinfo);
  long max_memory_to_use;
  long max_alloc_chunk;
} jpeg62_memory_mgr_t;


typedef struct jpeg62_common_struct_s {
  jpeg62_error_mgr_t* err;
  jpeg62_memory_mgr_t* mem;
  void* progress;   //struct jpeg_progress_mgr
  void* client_data;
  int is_decompressor;
  int global_state;
} jpeg62_common_struct_t;

typedef struct jpeg62_source_mgr_t {
  void *next_input_byte;
  size_t bytes_in_buffer;
  void (*init_source) (void* cinfo);
  int (*fill_input_buffer) (void* cinfo);
  void (*skip_input_data) (void* cinfo, long num_bytes);
  int (*resync_to_restart) (void* cinfo, int desired);
  void (*term_source) (void* cinfo);
} jpeg62_source_mgr_t;

// simplied structure
typedef struct j62_decompress_ptr_s
{
  jpeg62_error_mgr_t* err;
  jpeg62_memory_mgr_t* mem;
  void* progress;   //struct jpeg_progress_mgr
  void* client_data;
  int is_decompressor;
  int global_state;

  jpeg62_source_mgr_t *src;
  // other stuff not needed for wraping
} j62_decompress_ptr_t;

static struct __jmp_buf_tag jmpbuf;
static int                  is_jmpbuf;

static void wrapErrorMgr(jpeg62_error_mgr_t* mgr);
static void unwrapErrorMgr(jpeg62_error_mgr_t* mgr);
static void wrapMemoryMgr(jpeg62_memory_mgr_t* mgr);
static void unwrapMemoryMgr(jpeg62_memory_mgr_t* mgr);
static void wrapCommonStruct(jpeg62_common_struct_t* s, int type);
static void unwrapCommonStruct(jpeg62_common_struct_t* s, int type);

#define SUPER() \
GO(0)   \
GO(1)   \
GO(2)   \
GO(3)

static x86emu_t* my62_jpegcb_emu = NULL;\
// error_exit
#define GO(A)   \
static uintptr_t my_error_exit_fct_##A = 0;   \
static void my_error_exit_##A(jpeg62_common_struct_t* cinfo)        \
{                                                                   \
    uintptr_t oldip = my62_jpegcb_emu->ip.dword[0];                 \
    wrapCommonStruct(cinfo, 0);                                     \
    RunFunctionWithEmu(my62_jpegcb_emu, 1, my_error_exit_fct_##A, 1, cinfo);   \
    if(oldip==my62_jpegcb_emu->ip.dword[0])                         \
        unwrapCommonStruct(cinfo, 0);                               \
    else                                                            \
        if(is_jmpbuf) longjmp(&jmpbuf, 1);                          \
}
SUPER()
#undef GO
static void* finderror_exitFct(void* fct)
{
    if(!fct) return fct;
    if(GetNativeFnc((uintptr_t)fct))  return GetNativeFnc((uintptr_t)fct);
    #define GO(A) if(my_error_exit_fct_##A == (uintptr_t)fct) return my_error_exit_##A;
    SUPER()
    #undef GO
    #define GO(A) if(my_error_exit_fct_##A == 0) {my_error_exit_fct_##A = (uintptr_t)fct; return my_error_exit_##A; }
    SUPER()
    #undef GO
    printf_log(LOG_NONE, "Warning, no more slot for Jpeg error_exit callback\n");
    return NULL;
}
static void* reverse_error_exitFct(void* fct)
{
    if(!fct) return fct;
    if(CheckBridged(my_lib->priv.w.bridge, fct))
        return (void*)CheckBridged(my_lib->priv.w.bridge, fct);
    #define GO(A) if(my_error_exit_##A == fct) return (void*)my_error_exit_fct_##A;
    SUPER()
    #undef GO
    return (void*)AddBridge(my_lib->priv.w.bridge, my_lib->context, iFppp, fct, 0);
}

// emit_message
#define GO(A)   \
static uintptr_t my_emit_message_fct_##A = 0;   \
static void my_emit_message_##A(void* cinfo, int msg_level)     \
{                                       \
    RunFunctionWithEmu(my62_jpegcb_emu, 1, my_emit_message_fct_##A, 2, cinfo, msg_level);\
}
SUPER()
#undef GO
static void* findemit_messageFct(void* fct)
{
    if(!fct) return fct;
    if(GetNativeFnc((uintptr_t)fct))  return GetNativeFnc((uintptr_t)fct);
    #define GO(A) if(my_emit_message_fct_##A == (uintptr_t)fct) return my_emit_message_##A;
    SUPER()
    #undef GO
    #define GO(A) if(my_emit_message_fct_##A == 0) {my_emit_message_fct_##A = (uintptr_t)fct; return my_emit_message_##A; }
    SUPER()
    #undef GO
    printf_log(LOG_NONE, "Warning, no more slot for Jpeg emit_message callback\n");
    return NULL;
}
static void* reverse_emit_messageFct(void* fct)
{
    if(!fct) return fct;
    if(CheckBridged(my_lib->priv.w.bridge, fct))
        return (void*)CheckBridged(my_lib->priv.w.bridge, fct);
    #define GO(A) if(my_emit_message_##A == fct) return (void*)my_emit_message_fct_##A;
    SUPER()
    #undef GO
    return (void*)AddBridge(my_lib->priv.w.bridge, my_lib->context, iFppp, fct, 0);
}

// output_message
#define GO(A)   \
static uintptr_t my_output_message_fct_##A = 0;   \
static void my_output_message_##A(void* cinfo)     \
{                                       \
    RunFunctionWithEmu(my62_jpegcb_emu, 1, my_output_message_fct_##A, 1, cinfo);\
}
SUPER()
#undef GO
static void* findoutput_messageFct(void* fct)
{
    if(!fct) return fct;
    if(GetNativeFnc((uintptr_t)fct))  return GetNativeFnc((uintptr_t)fct);
    #define GO(A) if(my_output_message_fct_##A == (uintptr_t)fct) return my_output_message_##A;
    SUPER()
    #undef GO
    #define GO(A) if(my_output_message_fct_##A == 0) {my_output_message_fct_##A = (uintptr_t)fct; return my_output_message_##A; }
    SUPER()
    #undef GO
    printf_log(LOG_NONE, "Warning, no more slot for Jpeg output_message callback\n");
    return NULL;
}
static void* reverse_output_messageFct(void* fct)
{
    if(!fct) return fct;
    if(CheckBridged(my_lib->priv.w.bridge, fct))
        return (void*)CheckBridged(my_lib->priv.w.bridge, fct);
    #define GO(A) if(my_output_message_##A == fct) return (void*)my_output_message_fct_##A;
    SUPER()
    #undef GO
    return (void*)AddBridge(my_lib->priv.w.bridge, my_lib->context, iFppp, fct, 0);
}

// format_message
#define GO(A)   \
static uintptr_t my_format_message_fct_##A = 0;   \
static void my_format_message_##A(void* cinfo, char* buffer)     \
{                                       \
    RunFunctionWithEmu(my62_jpegcb_emu, 1, my_format_message_fct_##A, 2, cinfo, buffer);\
}
SUPER()
#undef GO
static void* findformat_messageFct(void* fct)
{
    if(!fct) return fct;
    if(GetNativeFnc((uintptr_t)fct))  return GetNativeFnc((uintptr_t)fct);
    #define GO(A) if(my_format_message_fct_##A == (uintptr_t)fct) return my_format_message_##A;
    SUPER()
    #undef GO
    #define GO(A) if(my_format_message_fct_##A == 0) {my_format_message_fct_##A = (uintptr_t)fct; return my_format_message_##A; }
    SUPER()
    #undef GO
    printf_log(LOG_NONE, "Warning, no more slot for Jpeg format_message callback\n");
    return NULL;
}
static void* reverse_format_messageFct(void* fct)
{
    if(!fct) return fct;
    if(CheckBridged(my_lib->priv.w.bridge, fct))
        return (void*)CheckBridged(my_lib->priv.w.bridge, fct);
    #define GO(A) if(my_format_message_##A == fct) return (void*)my_format_message_fct_##A;
    SUPER()
    #undef GO
    return (void*)AddBridge(my_lib->priv.w.bridge, my_lib->context, iFppp, fct, 0);
}

// reset_error_mgr
#define GO(A)   \
static uintptr_t my_reset_error_mgr_fct_##A = 0;   \
static void my_reset_error_mgr_##A(void* cinfo)     \
{                                       \
    RunFunctionWithEmu(my62_jpegcb_emu, 1, my_reset_error_mgr_fct_##A, 1, cinfo);\
}
SUPER()
#undef GO
static void* findreset_error_mgrFct(void* fct)
{
    if(!fct) return fct;
    if(GetNativeFnc((uintptr_t)fct))  return GetNativeFnc((uintptr_t)fct);
    #define GO(A) if(my_reset_error_mgr_fct_##A == (uintptr_t)fct) return my_reset_error_mgr_##A;
    SUPER()
    #undef GO
    #define GO(A) if(my_reset_error_mgr_fct_##A == 0) {my_reset_error_mgr_fct_##A = (uintptr_t)fct; return my_reset_error_mgr_##A; }
    SUPER()
    #undef GO
    printf_log(LOG_NONE, "Warning, no more slot for Jpeg reset_error_mgr callback\n");
    return NULL;
}
static void* reverse_reset_error_mgrFct(void* fct)
{
    if(!fct) return fct;
    if(CheckBridged(my_lib->priv.w.bridge, fct))
        return (void*)CheckBridged(my_lib->priv.w.bridge, fct);
    #define GO(A) if(my_reset_error_mgr_##A == fct) return (void*)my_reset_error_mgr_fct_##A;
    SUPER()
    #undef GO
    return (void*)AddBridge(my_lib->priv.w.bridge, my_lib->context, iFppp, fct, 0);
}

// jpeg_marker_parser_method
#define GO(A)   \
static uintptr_t my_jpeg_marker_parser_method_fct_##A = 0;   \
static int my_jpeg_marker_parser_method_##A(void* cinfo)    \
{                                       \
    return (int)RunFunction(my_context, my_jpeg_marker_parser_method_fct_##A, 1, cinfo);\
}
SUPER()
#undef GO
static void* findjpeg_marker_parser_methodFct(void* fct)
{
    if(!fct) return fct;
    if(GetNativeFnc((uintptr_t)fct))  return GetNativeFnc((uintptr_t)fct);
    #define GO(A) if(my_jpeg_marker_parser_method_fct_##A == (uintptr_t)fct) return my_jpeg_marker_parser_method_##A;
    SUPER()
    #undef GO
    #define GO(A) if(my_jpeg_marker_parser_method_fct_##A == 0) {my_jpeg_marker_parser_method_fct_##A = (uintptr_t)fct; return my_jpeg_marker_parser_method_##A; }
    SUPER()
    #undef GO
    printf_log(LOG_NONE, "Warning, no more slot for libjpeg.so.62 jpeg_marker_parser_method callback\n");
    return NULL;
}
static void* reverse_jpeg_marker_parser_methodFct(void* fct)
{
    if(!fct) return fct;
    if(CheckBridged(my_lib->priv.w.bridge, fct))
        return (void*)CheckBridged(my_lib->priv.w.bridge, fct);
    #define GO(A) if(my_jpeg_marker_parser_method_##A == fct) return (void*)my_jpeg_marker_parser_method_fct_##A;
    SUPER()
    #undef GO
    return (void*)AddBridge(my_lib->priv.w.bridge, my_lib->context, iFppp, fct, 0);
}

// alloc_small
#define GO(A)   \
static uintptr_t my_alloc_small_fct_##A = 0;   \
static void* my_alloc_small_##A(void* cinfo, int pool_id, size_t sizeofobject)    \
{                                       \
    return (void*)RunFunction(my_context, my_alloc_small_fct_##A, 3, cinfo, pool_id, sizeofobject);\
}
SUPER()
#undef GO
static void* findalloc_smallFct(void* fct)
{
    if(!fct) return fct;
    if(GetNativeFnc((uintptr_t)fct))  return GetNativeFnc((uintptr_t)fct);
    #define GO(A) if(my_alloc_small_fct_##A == (uintptr_t)fct) return my_alloc_small_##A;
    SUPER()
    #undef GO
    #define GO(A) if(my_alloc_small_fct_##A == 0) {my_alloc_small_fct_##A = (uintptr_t)fct; return my_alloc_small_##A; }
    SUPER()
    #undef GO
    printf_log(LOG_NONE, "Warning, no more slot for libjpeg.so.62 alloc_small callback\n");
    return NULL;
}
static void* reverse_alloc_smallFct(void* fct)
{
    if(!fct) return fct;
    if(CheckBridged(my_lib->priv.w.bridge, fct))
        return (void*)CheckBridged(my_lib->priv.w.bridge, fct);
    #define GO(A) if(my_alloc_small_##A == fct) return (void*)my_alloc_small_fct_##A;
    SUPER()
    #undef GO
    return (void*)AddBridge(my_lib->priv.w.bridge, my_lib->context, pFpiL, fct, 0);
}

// alloc_large
#define GO(A)   \
static uintptr_t my_alloc_large_fct_##A = 0;   \
static void* my_alloc_large_##A(void* cinfo, int pool_id, size_t sizeofobject)    \
{                                       \
    return (void*)RunFunction(my_context, my_alloc_large_fct_##A, 3, cinfo, pool_id, sizeofobject);\
}
SUPER()
#undef GO
static void* findalloc_largeFct(void* fct)
{
    if(!fct) return fct;
    if(GetNativeFnc((uintptr_t)fct))  return GetNativeFnc((uintptr_t)fct);
    #define GO(A) if(my_alloc_large_fct_##A == (uintptr_t)fct) return my_alloc_large_##A;
    SUPER()
    #undef GO
    #define GO(A) if(my_alloc_large_fct_##A == 0) {my_alloc_large_fct_##A = (uintptr_t)fct; return my_alloc_large_##A; }
    SUPER()
    #undef GO
    printf_log(LOG_NONE, "Warning, no more slot for libjpeg.so.62 alloc_large callback\n");
    return NULL;
}
static void* reverse_alloc_largeFct(void* fct)
{
    if(!fct) return fct;
    if(CheckBridged(my_lib->priv.w.bridge, fct))
        return (void*)CheckBridged(my_lib->priv.w.bridge, fct);
    #define GO(A) if(my_alloc_large_##A == fct) return (void*)my_alloc_large_fct_##A;
    SUPER()
    #undef GO
    return (void*)AddBridge(my_lib->priv.w.bridge, my_lib->context, pFpiL, fct, 0);
}

// alloc_sarray
#define GO(A)   \
static uintptr_t my_alloc_sarray_fct_##A = 0;   \
static void* my_alloc_sarray_##A(void* cinfo, int pool_id, uint32_t samplesperrow, uint32_t numrows)    \
{                                       \
    return (void*)RunFunction(my_context, my_alloc_sarray_fct_##A, 4, cinfo, pool_id, samplesperrow, numrows);\
}
SUPER()
#undef GO
static void* findalloc_sarrayFct(void* fct)
{
    if(!fct) return fct;
    if(GetNativeFnc((uintptr_t)fct))  return GetNativeFnc((uintptr_t)fct);
    #define GO(A) if(my_alloc_sarray_fct_##A == (uintptr_t)fct) return my_alloc_sarray_##A;
    SUPER()
    #undef GO
    #define GO(A) if(my_alloc_sarray_fct_##A == 0) {my_alloc_sarray_fct_##A = (uintptr_t)fct; return my_alloc_sarray_##A; }
    SUPER()
    #undef GO
    printf_log(LOG_NONE, "Warning, no more slot for libjpeg.so.62 alloc_sarray callback\n");
    return NULL;
}
static void* reverse_alloc_sarrayFct(void* fct)
{
    if(!fct) return fct;
    if(CheckBridged(my_lib->priv.w.bridge, fct))
        return (void*)CheckBridged(my_lib->priv.w.bridge, fct);
    #define GO(A) if(my_alloc_sarray_##A == fct) return (void*)my_alloc_sarray_fct_##A;
    SUPER()
    #undef GO
    return (void*)AddBridge(my_lib->priv.w.bridge, my_lib->context, pFpiuu, fct, 0);
}

// alloc_barray
#define GO(A)   \
static uintptr_t my_alloc_barray_fct_##A = 0;   \
static void* my_alloc_barray_##A(void* cinfo, int pool_id, uint32_t samplesperrow, uint32_t numrows)    \
{                                       \
    return (void*)RunFunction(my_context, my_alloc_barray_fct_##A, 4, cinfo, pool_id, samplesperrow, numrows);\
}
SUPER()
#undef GO
static void* findalloc_barrayFct(void* fct)
{
    if(!fct) return fct;
    if(GetNativeFnc((uintptr_t)fct))  return GetNativeFnc((uintptr_t)fct);
    #define GO(A) if(my_alloc_barray_fct_##A == (uintptr_t)fct) return my_alloc_barray_##A;
    SUPER()
    #undef GO
    #define GO(A) if(my_alloc_barray_fct_##A == 0) {my_alloc_barray_fct_##A = (uintptr_t)fct; return my_alloc_barray_##A; }
    SUPER()
    #undef GO
    printf_log(LOG_NONE, "Warning, no more slot for libjpeg.so.62 alloc_barray callback\n");
    return NULL;
}
static void* reverse_alloc_barrayFct(void* fct)
{
    if(!fct) return fct;
    if(CheckBridged(my_lib->priv.w.bridge, fct))
        return (void*)CheckBridged(my_lib->priv.w.bridge, fct);
    #define GO(A) if(my_alloc_barray_##A == fct) return (void*)my_alloc_barray_fct_##A;
    SUPER()
    #undef GO
    return (void*)AddBridge(my_lib->priv.w.bridge, my_lib->context, pFpiuu, fct, 0);
}

// request_virt_sarray
#define GO(A)   \
static uintptr_t my_request_virt_sarray_fct_##A = 0;   \
static void* my_request_virt_sarray_##A(void* cinfo, int pool_id, int pre_zero, uint32_t samplesperrow,uint32_t numrows, uint32_t maxaccess)    \
{                                       \
    return (void*)RunFunction(my_context, my_request_virt_sarray_fct_##A, 6, cinfo, pool_id, pre_zero, samplesperrow, numrows, maxaccess);\
}
SUPER()
#undef GO
static void* findrequest_virt_sarrayFct(void* fct)
{
    if(!fct) return fct;
    if(GetNativeFnc((uintptr_t)fct))  return GetNativeFnc((uintptr_t)fct);
    #define GO(A) if(my_request_virt_sarray_fct_##A == (uintptr_t)fct) return my_request_virt_sarray_##A;
    SUPER()
    #undef GO
    #define GO(A) if(my_request_virt_sarray_fct_##A == 0) {my_request_virt_sarray_fct_##A = (uintptr_t)fct; return my_request_virt_sarray_##A; }
    SUPER()
    #undef GO
    printf_log(LOG_NONE, "Warning, no more slot for libjpeg.so.62 request_virt_sarray callback\n");
    return NULL;
}
static void* reverse_request_virt_sarrayFct(void* fct)
{
    if(!fct) return fct;
    if(CheckBridged(my_lib->priv.w.bridge, fct))
        return (void*)CheckBridged(my_lib->priv.w.bridge, fct);
    #define GO(A) if(my_request_virt_sarray_##A == fct) return (void*)my_request_virt_sarray_fct_##A;
    SUPER()
    #undef GO
    return (void*)AddBridge(my_lib->priv.w.bridge, my_lib->context, pFpiiuuu, fct, 0);
}

// request_virt_barray
#define GO(A)   \
static uintptr_t my_request_virt_barray_fct_##A = 0;   \
static void* my_request_virt_barray_##A(void* cinfo, int pool_id, int pre_zero, uint32_t samplesperrow,uint32_t numrows, uint32_t maxaccess)    \
{                                       \
    return (void*)RunFunction(my_context, my_request_virt_barray_fct_##A, 6, cinfo, pool_id, pre_zero, samplesperrow, numrows, maxaccess);\
}
SUPER()
#undef GO
static void* findrequest_virt_barrayFct(void* fct)
{
    if(!fct) return fct;
    if(GetNativeFnc((uintptr_t)fct))  return GetNativeFnc((uintptr_t)fct);
    #define GO(A) if(my_request_virt_barray_fct_##A == (uintptr_t)fct) return my_request_virt_barray_##A;
    SUPER()
    #undef GO
    #define GO(A) if(my_request_virt_barray_fct_##A == 0) {my_request_virt_barray_fct_##A = (uintptr_t)fct; return my_request_virt_barray_##A; }
    SUPER()
    #undef GO
    printf_log(LOG_NONE, "Warning, no more slot for libjpeg.so.62 request_virt_barray callback\n");
    return NULL;
}
static void* reverse_request_virt_barrayFct(void* fct)
{
    if(!fct) return fct;
    if(CheckBridged(my_lib->priv.w.bridge, fct))
        return (void*)CheckBridged(my_lib->priv.w.bridge, fct);
    #define GO(A) if(my_request_virt_barray_##A == fct) return (void*)my_request_virt_barray_fct_##A;
    SUPER()
    #undef GO
    return (void*)AddBridge(my_lib->priv.w.bridge, my_lib->context, pFpiiuuu, fct, 0);
}

// realize_virt_arrays
#define GO(A)   \
static uintptr_t my_realize_virt_arrays_fct_##A = 0;   \
static void my_realize_virt_arrays_##A(void* cinfo)    \
{                                       \
    RunFunction(my_context, my_realize_virt_arrays_fct_##A, 1, cinfo);\
}
SUPER()
#undef GO
static void* findrealize_virt_arraysFct(void* fct)
{
    if(!fct) return fct;
    if(GetNativeFnc((uintptr_t)fct))  return GetNativeFnc((uintptr_t)fct);
    #define GO(A) if(my_realize_virt_arrays_fct_##A == (uintptr_t)fct) return my_realize_virt_arrays_##A;
    SUPER()
    #undef GO
    #define GO(A) if(my_realize_virt_arrays_fct_##A == 0) {my_realize_virt_arrays_fct_##A = (uintptr_t)fct; return my_realize_virt_arrays_##A; }
    SUPER()
    #undef GO
    printf_log(LOG_NONE, "Warning, no more slot for libjpeg.so.62 realize_virt_arrays callback\n");
    return NULL;
}
static void* reverse_realize_virt_arraysFct(void* fct)
{
    if(!fct) return fct;
    if(CheckBridged(my_lib->priv.w.bridge, fct))
        return (void*)CheckBridged(my_lib->priv.w.bridge, fct);
    #define GO(A) if(my_realize_virt_arrays_##A == fct) return (void*)my_realize_virt_arrays_fct_##A;
    SUPER()
    #undef GO
    return (void*)AddBridge(my_lib->priv.w.bridge, my_lib->context, vFp, fct, 0);
}

// access_virt_sarray
#define GO(A)   \
static uintptr_t my_access_virt_sarray_fct_##A = 0;   \
static void* my_access_virt_sarray_##A(void* cinfo, void* ptr, uint32_t start_row, uint32_t num_rows, int writable)    \
{                                       \
    return (void*)RunFunction(my_context, my_access_virt_sarray_fct_##A, 5, cinfo, ptr, start_row, num_rows, writable);\
}
SUPER()
#undef GO
static void* findaccess_virt_sarrayFct(void* fct)
{
    if(!fct) return fct;
    if(GetNativeFnc((uintptr_t)fct))  return GetNativeFnc((uintptr_t)fct);
    #define GO(A) if(my_access_virt_sarray_fct_##A == (uintptr_t)fct) return my_access_virt_sarray_##A;
    SUPER()
    #undef GO
    #define GO(A) if(my_access_virt_sarray_fct_##A == 0) {my_access_virt_sarray_fct_##A = (uintptr_t)fct; return my_access_virt_sarray_##A; }
    SUPER()
    #undef GO
    printf_log(LOG_NONE, "Warning, no more slot for libjpeg.so.62 access_virt_sarray callback\n");
    return NULL;
}
static void* reverse_access_virt_sarrayFct(void* fct)
{
    if(!fct) return fct;
    if(CheckBridged(my_lib->priv.w.bridge, fct))
        return (void*)CheckBridged(my_lib->priv.w.bridge, fct);
    #define GO(A) if(my_access_virt_sarray_##A == fct) return (void*)my_access_virt_sarray_fct_##A;
    SUPER()
    #undef GO
    return (void*)AddBridge(my_lib->priv.w.bridge, my_lib->context, pFppuui, fct, 0);
}

// access_virt_barray
#define GO(A)   \
static uintptr_t my_access_virt_barray_fct_##A = 0;   \
static void* my_access_virt_barray_##A(void* cinfo, void* ptr, uint32_t start_row, uint32_t num_rows, int writable)    \
{                                       \
    return (void*)RunFunction(my_context, my_access_virt_barray_fct_##A, 5, cinfo, ptr, start_row, num_rows, writable);\
}
SUPER()
#undef GO
static void* findaccess_virt_barrayFct(void* fct)
{
    if(!fct) return fct;
    if(GetNativeFnc((uintptr_t)fct))  return GetNativeFnc((uintptr_t)fct);
    #define GO(A) if(my_access_virt_barray_fct_##A == (uintptr_t)fct) return my_access_virt_barray_##A;
    SUPER()
    #undef GO
    #define GO(A) if(my_access_virt_barray_fct_##A == 0) {my_access_virt_barray_fct_##A = (uintptr_t)fct; return my_access_virt_barray_##A; }
    SUPER()
    #undef GO
    printf_log(LOG_NONE, "Warning, no more slot for libjpeg.so.62 access_virt_barray callback\n");
    return NULL;
}
static void* reverse_access_virt_barrayFct(void* fct)
{
    if(!fct) return fct;
    if(CheckBridged(my_lib->priv.w.bridge, fct))
        return (void*)CheckBridged(my_lib->priv.w.bridge, fct);
    #define GO(A) if(my_access_virt_barray_##A == fct) return (void*)my_access_virt_barray_fct_##A;
    SUPER()
    #undef GO
    return (void*)AddBridge(my_lib->priv.w.bridge, my_lib->context, pFppuui, fct, 0);
}

// free_pool
#define GO(A)   \
static uintptr_t my_free_pool_fct_##A = 0;   \
static void my_free_pool_##A(void* cinfo, int pool_id)    \
{                                       \
    RunFunction(my_context, my_free_pool_fct_##A, 2, cinfo, pool_id);\
}
SUPER()
#undef GO
static void* findfree_poolFct(void* fct)
{
    if(!fct) return fct;
    if(GetNativeFnc((uintptr_t)fct))  return GetNativeFnc((uintptr_t)fct);
    #define GO(A) if(my_free_pool_fct_##A == (uintptr_t)fct) return my_free_pool_##A;
    SUPER()
    #undef GO
    #define GO(A) if(my_free_pool_fct_##A == 0) {my_free_pool_fct_##A = (uintptr_t)fct; return my_free_pool_##A; }
    SUPER()
    #undef GO
    printf_log(LOG_NONE, "Warning, no more slot for libjpeg.so.62 free_pool callback\n");
    return NULL;
}
static void* reverse_free_poolFct(void* fct)
{
    if(!fct) return fct;
    if(CheckBridged(my_lib->priv.w.bridge, fct))
        return (void*)CheckBridged(my_lib->priv.w.bridge, fct);
    #define GO(A) if(my_free_pool_##A == fct) return (void*)my_free_pool_fct_##A;
    SUPER()
    #undef GO
    return (void*)AddBridge(my_lib->priv.w.bridge, my_lib->context, vFpi, fct, 0);
}

// self_destruct
#define GO(A)   \
static uintptr_t my_self_destruct_fct_##A = 0;   \
static void my_self_destruct_##A(void* cinfo)    \
{                                       \
    RunFunction(my_context, my_self_destruct_fct_##A, 1, cinfo);\
}
SUPER()
#undef GO
static void* findself_destructFct(void* fct)
{
    if(!fct) return fct;
    if(GetNativeFnc((uintptr_t)fct))  return GetNativeFnc((uintptr_t)fct);
    #define GO(A) if(my_self_destruct_fct_##A == (uintptr_t)fct) return my_self_destruct_##A;
    SUPER()
    #undef GO
    #define GO(A) if(my_self_destruct_fct_##A == 0) {my_self_destruct_fct_##A = (uintptr_t)fct; return my_self_destruct_##A; }
    SUPER()
    #undef GO
    printf_log(LOG_NONE, "Warning, no more slot for libjpeg.so.62 self_destruct callback\n");
    return NULL;
}
static void* reverse_self_destructFct(void* fct)
{
    if(!fct) return fct;
    if(CheckBridged(my_lib->priv.w.bridge, fct))
        return (void*)CheckBridged(my_lib->priv.w.bridge, fct);
    #define GO(A) if(my_self_destruct_##A == fct) return (void*)my_self_destruct_fct_##A;
    SUPER()
    #undef GO
    return (void*)AddBridge(my_lib->priv.w.bridge, my_lib->context, vFp, fct, 0);
}

// init_source
#define GO(A)   \
static uintptr_t my_init_source_fct_##A = 0;   \
static void my_init_source_##A(void* cinfo)    \
{                                       \
    RunFunction(my_context, my_init_source_fct_##A, 1, cinfo);\
}
SUPER()
#undef GO
static void* findinit_sourceFct(void* fct)
{
    if(!fct) return fct;
    if(GetNativeFnc((uintptr_t)fct))  return GetNativeFnc((uintptr_t)fct);
    #define GO(A) if(my_init_source_fct_##A == (uintptr_t)fct) return my_init_source_##A;
    SUPER()
    #undef GO
    #define GO(A) if(my_init_source_fct_##A == 0) {my_init_source_fct_##A = (uintptr_t)fct; return my_init_source_##A; }
    SUPER()
    #undef GO
    printf_log(LOG_NONE, "Warning, no more slot for libjpeg.so.62 init_source callback\n");
    return NULL;
}
static void* reverse_init_sourceFct(void* fct)
{
    if(!fct) return fct;
    if(CheckBridged(my_lib->priv.w.bridge, fct))
        return (void*)CheckBridged(my_lib->priv.w.bridge, fct);
    #define GO(A) if(my_init_source_##A == fct) return (void*)my_init_source_fct_##A;
    SUPER()
    #undef GO
    return (void*)AddBridge(my_lib->priv.w.bridge, my_lib->context, vFp, fct, 0);
}

// fill_input_buffer
#define GO(A)   \
static uintptr_t my_fill_input_buffer_fct_##A = 0;   \
static int my_fill_input_buffer_##A(void* cinfo)    \
{                                       \
    return (int)RunFunction(my_context, my_fill_input_buffer_fct_##A, 1, cinfo);\
}
SUPER()
#undef GO
static void* findfill_input_bufferFct(void* fct)
{
    if(!fct) return fct;
    if(GetNativeFnc((uintptr_t)fct))  return GetNativeFnc((uintptr_t)fct);
    #define GO(A) if(my_fill_input_buffer_fct_##A == (uintptr_t)fct) return my_fill_input_buffer_##A;
    SUPER()
    #undef GO
    #define GO(A) if(my_fill_input_buffer_fct_##A == 0) {my_fill_input_buffer_fct_##A = (uintptr_t)fct; return my_fill_input_buffer_##A; }
    SUPER()
    #undef GO
    printf_log(LOG_NONE, "Warning, no more slot for libjpeg.so.62 fill_input_buffer callback\n");
    return NULL;
}
static void* reverse_fill_input_bufferFct(void* fct)
{
    if(!fct) return fct;
    if(CheckBridged(my_lib->priv.w.bridge, fct))
        return (void*)CheckBridged(my_lib->priv.w.bridge, fct);
    #define GO(A) if(my_fill_input_buffer_##A == fct) return (void*)my_fill_input_buffer_fct_##A;
    SUPER()
    #undef GO
    return (void*)AddBridge(my_lib->priv.w.bridge, my_lib->context, iFp, fct, 0);
}

// skip_input_data
#define GO(A)   \
static uintptr_t my_skip_input_data_fct_##A = 0;   \
static void my_skip_input_data_##A(void* cinfo, long num_bytes)    \
{                                       \
    RunFunction(my_context, my_skip_input_data_fct_##A, 2, cinfo, num_bytes);\
}
SUPER()
#undef GO
static void* findskip_input_dataFct(void* fct)
{
    if(!fct) return fct;
    if(GetNativeFnc((uintptr_t)fct))  return GetNativeFnc((uintptr_t)fct);
    #define GO(A) if(my_skip_input_data_fct_##A == (uintptr_t)fct) return my_skip_input_data_##A;
    SUPER()
    #undef GO
    #define GO(A) if(my_skip_input_data_fct_##A == 0) {my_skip_input_data_fct_##A = (uintptr_t)fct; return my_skip_input_data_##A; }
    SUPER()
    #undef GO
    printf_log(LOG_NONE, "Warning, no more slot for libjpeg.so.62 skip_input_data callback\n");
    return NULL;
}
static void* reverse_skip_input_dataFct(void* fct)
{
    if(!fct) return fct;
    if(CheckBridged(my_lib->priv.w.bridge, fct))
        return (void*)CheckBridged(my_lib->priv.w.bridge, fct);
    #define GO(A) if(my_skip_input_data_##A == fct) return (void*)my_skip_input_data_fct_##A;
    SUPER()
    #undef GO
    return (void*)AddBridge(my_lib->priv.w.bridge, my_lib->context, vFpl, fct, 0);
}

// resync_to_restart
#define GO(A)   \
static uintptr_t my_resync_to_restart_fct_##A = 0;   \
static int my_resync_to_restart_##A(void* cinfo, int desired)    \
{                                       \
    return (int)RunFunction(my_context, my_resync_to_restart_fct_##A, 2, cinfo, desired);\
}
SUPER()
#undef GO
static void* findresync_to_restartFct(void* fct)
{
    if(!fct) return fct;
    if(GetNativeFnc((uintptr_t)fct))  return GetNativeFnc((uintptr_t)fct);
    #define GO(A) if(my_resync_to_restart_fct_##A == (uintptr_t)fct) return my_resync_to_restart_##A;
    SUPER()
    #undef GO
    #define GO(A) if(my_resync_to_restart_fct_##A == 0) {my_resync_to_restart_fct_##A = (uintptr_t)fct; return my_resync_to_restart_##A; }
    SUPER()
    #undef GO
    printf_log(LOG_NONE, "Warning, no more slot for libjpeg.so.62 resync_to_restart callback\n");
    return NULL;
}
static void* reverse_resync_to_restartFct(void* fct)
{
    if(!fct) return fct;
    if(CheckBridged(my_lib->priv.w.bridge, fct))
        return (void*)CheckBridged(my_lib->priv.w.bridge, fct);
    #define GO(A) if(my_resync_to_restart_##A == fct) return (void*)my_resync_to_restart_fct_##A;
    SUPER()
    #undef GO
    return (void*)AddBridge(my_lib->priv.w.bridge, my_lib->context, iFpi, fct, 0);
}

// term_source
#define GO(A)   \
static uintptr_t my_term_source_fct_##A = 0;   \
static void my_term_source_##A(void* cinfo)    \
{                                       \
    RunFunction(my_context, my_term_source_fct_##A, 1, cinfo);\
}
SUPER()
#undef GO
static void* findterm_sourceFct(void* fct)
{
    if(!fct) return fct;
    if(GetNativeFnc((uintptr_t)fct))  return GetNativeFnc((uintptr_t)fct);
    #define GO(A) if(my_term_source_fct_##A == (uintptr_t)fct) return my_term_source_##A;
    SUPER()
    #undef GO
    #define GO(A) if(my_term_source_fct_##A == 0) {my_term_source_fct_##A = (uintptr_t)fct; return my_term_source_##A; }
    SUPER()
    #undef GO
    printf_log(LOG_NONE, "Warning, no more slot for libjpeg.so.62 term_source callback\n");
    return NULL;
}
static void* reverse_term_sourceFct(void* fct)
{
    if(!fct) return fct;
    if(CheckBridged(my_lib->priv.w.bridge, fct))
        return (void*)CheckBridged(my_lib->priv.w.bridge, fct);
    #define GO(A) if(my_term_source_##A == fct) return (void*)my_term_source_fct_##A;
    SUPER()
    #undef GO
    return (void*)AddBridge(my_lib->priv.w.bridge, my_lib->context, vFp, fct, 0);
}


#undef SUPER

#define SUPER()         \
    GO(error_exit)      \
    GO(emit_message)    \
    GO(output_message)  \
    GO(format_message)  \
    GO(reset_error_mgr)

static void wrapErrorMgr(jpeg62_error_mgr_t* mgr)
{
    if(!mgr)
        return;

    #define GO(A) mgr->A = reverse_##A##Fct(mgr->A);

    SUPER()
    #undef GO
}

static void unwrapErrorMgr(jpeg62_error_mgr_t* mgr)
{
    if(!mgr)
        return;

    #define GO(A)    mgr->A = find##A##Fct(mgr->A);
        
    SUPER()
    #undef GO
}
#undef SUPER

#define SUPER()             \
    GO(alloc_small)         \
    GO(alloc_large)         \
    GO(alloc_sarray)        \
    GO(alloc_barray)        \
    GO(request_virt_sarray) \
    GO(request_virt_barray) \
    GO(realize_virt_arrays) \
    GO(access_virt_sarray)  \
    GO(access_virt_barray)  \
    GO(free_pool)           \
    GO(self_destruct)

static void wrapMemoryMgr(jpeg62_memory_mgr_t* mgr)
{
    if(!mgr || (uintptr_t)mgr<0x1000)
        return;

    #define GO(A) mgr->A = reverse_##A##Fct(mgr->A);

    SUPER()
    #undef GO
}

static void unwrapMemoryMgr(jpeg62_memory_mgr_t* mgr)
{
    if(!mgr || (uintptr_t)mgr<0x1000)
        return;

    #define GO(A)    mgr->A = find##A##Fct(mgr->A);
        
    SUPER()
    #undef GO
}
#undef SUPER

#define SUPER()             \
    GO(init_source)         \
    GO(fill_input_buffer)   \
    GO(skip_input_data)     \
    GO(resync_to_restart)   \
    GO(term_source)

static void wrapSourceMgr(jpeg62_source_mgr_t* mgr)
{
    if(!mgr)
        return;

    #define GO(A) mgr->A = reverse_##A##Fct(mgr->A);

    SUPER()
    #undef GO
}

static void unwrapSourceMgr(jpeg62_source_mgr_t* mgr)
{
    if(!mgr)
        return;

    #define GO(A)    mgr->A = find##A##Fct(mgr->A);
        
    SUPER()
    #undef GO
}
#undef SUPER

static void wrapCommonStruct(jpeg62_common_struct_t* s, int type)
{
    wrapErrorMgr(s->err);
    wrapMemoryMgr(s->mem);
    if(type==1)
        wrapSourceMgr(((j62_decompress_ptr_t*)s)->src);
}
static void unwrapCommonStruct(jpeg62_common_struct_t* s, int type)
{
    unwrapErrorMgr(s->err);
    unwrapMemoryMgr(s->mem);
    if(type==1)
        unwrapSourceMgr(((j62_decompress_ptr_t*)s)->src);
}


EXPORT int my62_jpeg_simd_cpu_support()
{
    return 0x01|0x04|0x08; // MMX/SSE/SSE2
}

EXPORT void* my62_jpeg_std_error(x86emu_t* emu, void* errmgr)
{
    jpeg62_my_t *my = (jpeg62_my_t*)my_lib->priv.w.p2;

    jpeg62_error_mgr_t* ret = my->jpeg_std_error(errmgr);

    wrapErrorMgr(ret);
trace_end = 0;

    return ret;
}

#define WRAP(R, A, T)               \
    jpeg62_my_t *my = (jpeg62_my_t*)my_lib->priv.w.p2;  \
    is_jmpbuf = 1;                  \
    my62_jpegcb_emu = emu;          \
    unwrapCommonStruct(cinfo, T);   \
    if(setjmp(&jmpbuf)) {           \
        wrapCommonStruct(cinfo, T); \
        is_jmpbuf = 0;              \
        return (R)R_EAX;            \
    }                               \
    A;                              \
    is_jmpbuf = 0;                  \
    wrapCommonStruct(cinfo, T)


EXPORT void my62_jpeg_CreateDecompress(x86emu_t* emu, jpeg62_common_struct_t* cinfo, int version, unsigned long structsize)
{
    // Not using WRAP macro because only err field might be initialized here
    jpeg62_my_t *my = (jpeg62_my_t*)my_lib->priv.w.p2;
    is_jmpbuf = 1;
    my62_jpegcb_emu = emu;
    unwrapErrorMgr(cinfo->err);
    if(setjmp(&jmpbuf)) {
        wrapErrorMgr(cinfo->err);
        is_jmpbuf = 0;
        return;
    }
    my->jpeg_CreateDecompress(cinfo, version, structsize);
    is_jmpbuf = 0;
    wrapCommonStruct(cinfo, 1);
}

EXPORT int my62_jpeg_read_header(x86emu_t* emu, jpeg62_common_struct_t* cinfo, int image)
{
    WRAP(int, int ret = my->jpeg_read_header(cinfo, image), 1);
    return ret;
}

EXPORT int my62_jpeg_start_decompress(x86emu_t* emu, jpeg62_common_struct_t* cinfo)
{
    WRAP(int, int ret = my->jpeg_start_decompress(cinfo), 1);
    return ret;
}

EXPORT uint32_t my62_jpeg_read_scanlines(x86emu_t* emu, jpeg62_common_struct_t* cinfo, void* scanlines, uint32_t maxlines)
{
    WRAP(uint32_t, uint32_t ret = my->jpeg_read_scanlines(cinfo, scanlines, maxlines), 1);
    return ret;
}

EXPORT int my62_jpeg_finish_decompress(x86emu_t* emu, jpeg62_common_struct_t* cinfo)
{
    WRAP(int, int ret = my->jpeg_finish_decompress(cinfo), 1);
    return ret;
}

EXPORT void my62_jpeg_set_marker_processor(x86emu_t* emu, jpeg62_common_struct_t* cinfo, int marker, void* routine)
{
    WRAP(void, my->jpeg_set_marker_processor(cinfo, marker, findjpeg_marker_parser_methodFct(routine)), 0);
}

EXPORT void my62_jpeg_destroy_decompress(x86emu_t* emu, jpeg62_common_struct_t* cinfo)
{
    // no WRAP macro because we don't want to wrap at the exit
    jpeg62_my_t *my = (jpeg62_my_t*)my_lib->priv.w.p2;
    is_jmpbuf = 1;
    my62_jpegcb_emu = emu;
    unwrapCommonStruct(cinfo, 1);
    if(setjmp(&jmpbuf)) {
        wrapCommonStruct(cinfo, 1); // error, so re-wrap
        is_jmpbuf = 0;
        return;
    }
    my->jpeg_destroy_decompress(cinfo);
    is_jmpbuf = 0;
}

#undef WRAP

#define CUSTOM_INIT \
    my_bridge = lib->priv.w.bridge;     \
    my_lib = lib;                       \
    lib->altmy = strdup("my62_");       \
    lib->priv.w.p2 = getJpeg62My(lib);

#define CUSTOM_FINI \
    freeJpeg62My(lib->priv.w.p2);   \
    free(lib->priv.w.p2);           \
    my_lib = NULL;

#include "wrappedlib_init.h"
