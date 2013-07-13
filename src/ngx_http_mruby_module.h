/*
// ngx_http_mruby_module.h - ngx_mruby mruby module header
//
// See Copyright Notice in ngx_http_mruby_module.c
*/

#ifndef NGX_HTTP_MRUBY_MODULE_H
#define NGX_HTTP_MRUBY_MODULE_H

#include <ngx_core.h>
#include <ngx_http.h>
#include <nginx.h>

#include "ngx_http_mruby_core.h"
#include "ngx_http_mruby_init.h"

#if defined(NDK) && NDK
typedef struct {
    size_t           size;
    ngx_str_t        script;
    ngx_mrb_state_t *state;
    ngx_mrb_code_t  *code;
} ngx_http_mruby_set_var_data_t;
#include <ndk.h>
#endif

#define MODULE_NAME        "ngx_mruby"
#define MODULE_VERSION     "0.0.1"

extern ngx_module_t  ngx_http_mruby_module;

typedef struct ngx_http_mruby_main_conf_t {
    ngx_mrb_state_t *state;
} ngx_http_mruby_main_conf_t;

typedef struct ngx_http_mruby_loc_conf_t {
    ngx_mrb_code_t *post_read_code;
    ngx_mrb_code_t *server_rewrite_code;
    ngx_mrb_code_t *rewrite_code;
    ngx_mrb_code_t *access_code;
    ngx_mrb_code_t *handler_code;
    ngx_mrb_code_t *log_handler_code;
    ngx_mrb_code_t *post_read_inline_code;
    ngx_mrb_code_t *server_rewrite_inline_code;
    ngx_mrb_code_t *rewrite_inline_code;
    ngx_mrb_code_t *access_inline_code;
    ngx_mrb_code_t *content_inline_code;
    ngx_mrb_code_t *log_inline_code;
    ngx_flag_t      cached;
} ngx_http_mruby_loc_conf_t;

#endif // NGX_HTTP_MRUBY_MODULE_H
