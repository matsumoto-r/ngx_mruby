/*
// ngx_http_mruby.c - ngx_mruby mruby module
//
// See Copyright Notice in ngx_http_mruby_module.c
*/

#include "ngx_http_mruby.h"

#include <mruby.h>
#include <mruby/proc.h>
#include <mruby/data.h>
#include <mruby/compile.h>
#include <mruby/string.h>

ngx_http_request_t *ngx_mruby_request_state;

static void ngx_mrb_raise_error(mrb_state *mrb, mrb_value obj, ngx_http_request_t *r);
static void ngx_mrb_raise_file_error(mrb_state *mrb, mrb_value obj, ngx_http_request_t *r, char *code_file);
static ngx_int_t ngx_mrb_push_request(ngx_http_request_t *r);
static ngx_http_request_t *ngx_mrb_get_request(void);
static ngx_int_t ngx_mrb_class_init(mrb_state *mrb);
static mrb_value ngx_mrb_send_header(mrb_state *mrb, mrb_value self);
static mrb_value ngx_mrb_rputs(mrb_state *mrb, mrb_value self);
static mrb_value ngx_mrb_get_content_type(mrb_state *mrb, mrb_value self);
static mrb_value ngx_mrb_set_content_type(mrb_state *mrb, mrb_value self);
static mrb_value ngx_mrb_get_request_uri(mrb_state *mrb, mrb_value str);

ngx_int_t ngx_mrb_init_file(char *code_file_path, size_t len, ngx_mrb_state_t *state)
{
    FILE *mrb_file;
    mrb_state *mrb;
    struct mrb_parser_state *p;

    if ((mrb_file = fopen((char *)code_file_path, "r")) == NULL) {
        return NGX_ERROR;
    }

    mrb = mrb_open();
    ngx_mrb_class_init(mrb);

    state->ai  = mrb_gc_arena_save(mrb);
    p          = mrb_parse_file(mrb, mrb_file, NULL);
    state->mrb = mrb;
    state->n   = mrb_generate_code(mrb, p);
    ngx_cpystrn((u_char *)state->file, (u_char *)code_file_path, len + 1);
    mrb_pool_close(p->pool);
    fclose(mrb_file);
    return NGX_OK;
}

ngx_int_t ngx_mrb_init_string(char *code, ngx_mrb_state_t *state)
{
    mrb_state *mrb;
    struct mrb_parser_state *p;

    mrb = mrb_open();
    ngx_mrb_class_init(mrb);

    state->ai   = mrb_gc_arena_save(mrb);
    p           = mrb_parse_string(mrb, code, NULL);
    state->mrb  = mrb;
    state->n    = mrb_generate_code(mrb, p);
    state->file = NGX_CONF_UNSET_PTR;
    mrb_pool_close(p->pool);
    return NGX_OK;
}

ngx_int_t ngx_mrb_run(ngx_http_request_t *r, ngx_mrb_state_t *state)
{
    if (state == NGX_CONF_UNSET_PTR) {
        return NGX_DECLINED;
    }
    ngx_mrb_push_request(r);
    mrb_run(state->mrb, mrb_proc_new(state->mrb, state->mrb->irep[state->n]), mrb_nil_value());
    if (state->mrb->exc) {
        if (state->file != NGX_CONF_UNSET_PTR) {
            ngx_mrb_raise_file_error(state->mrb, mrb_obj_value(state->mrb->exc), r, state->file);
        } else {
            ngx_mrb_raise_error(state->mrb, mrb_obj_value(state->mrb->exc), r);
        }
    }
    mrb_gc_arena_restore(state->mrb, state->ai);
    return NGX_OK;
}

static void ngx_mrb_raise_error(mrb_state *mrb, mrb_value obj, ngx_http_request_t *r)
{  
    struct RString *str;
    char *err_out;
    
    obj = mrb_funcall(mrb, obj, "inspect", 0);
    
    if (mrb_type(obj) == MRB_TT_STRING) {
        str = mrb_str_ptr(obj);
        err_out = str->ptr;
        ngx_log_error(NGX_LOG_ERR
            , r->connection->log
            , 0
            , "mrb_run failed. error: %s"
            , err_out
        );
    }
}

static void ngx_mrb_raise_file_error(mrb_state *mrb, mrb_value obj, ngx_http_request_t *r, char *code_file)
{  
    struct RString *str;
    char *err_out;
    
    obj = mrb_funcall(mrb, obj, "inspect", 0);
    
    if (mrb_type(obj) == MRB_TT_STRING) {
        str = mrb_str_ptr(obj);
        err_out = str->ptr;
        ngx_log_error(NGX_LOG_ERR
                      , r->connection->log
                      , 0
                      , "mrb_run failed. file: %s error: %s"
                      , code_file
                      , err_out
                      );
    }
}

static ngx_int_t ngx_mrb_push_request(ngx_http_request_t *r)
{
    ngx_mruby_request_state = r;
    return NGX_OK;
}

static ngx_http_request_t *ngx_mrb_get_request(void)
{
    return ngx_mruby_request_state;
}

static ngx_int_t ngx_mrb_class_init(mrb_state *mrb)
{
    struct RClass *class;
    struct RClass *class_request;

    class = mrb_define_module(mrb, "Nginx");
    mrb_define_const(mrb, class, "NGX_OK", mrb_fixnum_value(NGX_OK));
    mrb_define_const(mrb, class, "NGX_ERROR", mrb_fixnum_value(NGX_ERROR));
    mrb_define_const(mrb, class, "NGX_AGAIN", mrb_fixnum_value(NGX_AGAIN));
    mrb_define_const(mrb, class, "NGX_BUSY", mrb_fixnum_value(NGX_BUSY));
    mrb_define_const(mrb, class, "NGX_DONE", mrb_fixnum_value(NGX_DONE));
    mrb_define_const(mrb, class, "NGX_DECLINED", mrb_fixnum_value(NGX_DECLINED));
    mrb_define_const(mrb, class, "NGX_ABORT", mrb_fixnum_value(NGX_ABORT));
    mrb_define_const(mrb, class, "NGX_HTTP_OK", mrb_fixnum_value(NGX_HTTP_OK));
    mrb_define_const(mrb, class, "NGX_HTTP_CREATED", mrb_fixnum_value(NGX_HTTP_CREATED));
    mrb_define_const(mrb, class, "NGX_HTTP_ACCEPTED", mrb_fixnum_value(NGX_HTTP_ACCEPTED));
    mrb_define_const(mrb, class, "NGX_HTTP_NO_CONTENT", mrb_fixnum_value(NGX_HTTP_NO_CONTENT));
    mrb_define_const(mrb, class, "NGX_HTTP_SPECIAL_RESPONSE", mrb_fixnum_value(NGX_HTTP_SPECIAL_RESPONSE));
    mrb_define_const(mrb, class, "NGX_HTTP_MOVED_PERMANENTLY", mrb_fixnum_value(NGX_HTTP_MOVED_PERMANENTLY));
    mrb_define_const(mrb, class, "NGX_HTTP_MOVED_TEMPORARILY", mrb_fixnum_value(NGX_HTTP_MOVED_TEMPORARILY));
    mrb_define_const(mrb, class, "NGX_HTTP_SEE_OTHER", mrb_fixnum_value(NGX_HTTP_SEE_OTHER));
    mrb_define_const(mrb, class, "NGX_HTTP_NOT_MODIFIED", mrb_fixnum_value(NGX_HTTP_NOT_MODIFIED));
    mrb_define_const(mrb, class, "NGX_HTTP_TEMPORARY_REDIRECT", mrb_fixnum_value(NGX_HTTP_TEMPORARY_REDIRECT));
    mrb_define_const(mrb, class, "NGX_HTTP_BAD_REQUEST", mrb_fixnum_value(NGX_HTTP_BAD_REQUEST));
    mrb_define_const(mrb, class, "NGX_HTTP_UNAUTHORIZED", mrb_fixnum_value(NGX_HTTP_UNAUTHORIZED));
    mrb_define_const(mrb, class, "NGX_HTTP_FORBIDDEN", mrb_fixnum_value(NGX_HTTP_FORBIDDEN));
    mrb_define_const(mrb, class, "NGX_HTTP_NOT_FOUND", mrb_fixnum_value(NGX_HTTP_NOT_FOUND));
    mrb_define_const(mrb, class, "NGX_HTTP_NOT_ALLOWED", mrb_fixnum_value(NGX_HTTP_NOT_ALLOWED));
    mrb_define_const(mrb, class, "NGX_HTTP_REQUEST_TIME_OUT", mrb_fixnum_value(NGX_HTTP_REQUEST_TIME_OUT));
    mrb_define_const(mrb, class, "NGX_HTTP_CONFLICT", mrb_fixnum_value(NGX_HTTP_CONFLICT));
    mrb_define_const(mrb, class, "NGX_HTTP_LENGTH_REQUIRED", mrb_fixnum_value(NGX_HTTP_LENGTH_REQUIRED));
    mrb_define_const(mrb, class, "NGX_HTTP_PRECONDITION_FAILED", mrb_fixnum_value(NGX_HTTP_PRECONDITION_FAILED));
    mrb_define_const(mrb, class, "NGX_HTTP_REQUEST_ENTITY_TOO_LARGE", mrb_fixnum_value(NGX_HTTP_REQUEST_ENTITY_TOO_LARGE));
    mrb_define_const(mrb, class, "NGX_HTTP_REQUEST_URI_TOO_LARGE", mrb_fixnum_value(NGX_HTTP_REQUEST_URI_TOO_LARGE));
    mrb_define_const(mrb, class, "NGX_HTTP_UNSUPPORTED_MEDIA_TYPE", mrb_fixnum_value(NGX_HTTP_UNSUPPORTED_MEDIA_TYPE));
    mrb_define_const(mrb, class, "NGX_HTTP_RANGE_NOT_SATISFIABLE", mrb_fixnum_value(NGX_HTTP_RANGE_NOT_SATISFIABLE));
    mrb_define_const(mrb, class, "NGX_HTTP_CLOSE", mrb_fixnum_value(NGX_HTTP_CLOSE));
    mrb_define_const(mrb, class, "NGX_HTTP_NGINX_CODES", mrb_fixnum_value(NGX_HTTP_NGINX_CODES));
    mrb_define_const(mrb, class, "NGX_HTTP_REQUEST_HEADER_TOO_LARGE", mrb_fixnum_value(NGX_HTTP_REQUEST_HEADER_TOO_LARGE));
    mrb_define_const(mrb, class, "NGX_HTTPS_CERT_ERROR", mrb_fixnum_value(NGX_HTTPS_CERT_ERROR));
    mrb_define_const(mrb, class, "NGX_HTTPS_NO_CERT", mrb_fixnum_value(NGX_HTTPS_NO_CERT));
    mrb_define_const(mrb, class, "NGX_HTTP_TO_HTTPS", mrb_fixnum_value(NGX_HTTP_TO_HTTPS));
    mrb_define_const(mrb, class, "NGX_HTTP_CLIENT_CLOSED_REQUEST", mrb_fixnum_value(NGX_HTTP_CLIENT_CLOSED_REQUEST));
    mrb_define_const(mrb, class, "NGX_HTTP_INTERNAL_SERVER_ERROR", mrb_fixnum_value(NGX_HTTP_INTERNAL_SERVER_ERROR));
    mrb_define_const(mrb, class, "NGX_HTTP_NOT_IMPLEMENTED", mrb_fixnum_value(NGX_HTTP_NOT_IMPLEMENTED));
    mrb_define_const(mrb, class, "NGX_HTTP_BAD_GATEWAY", mrb_fixnum_value(NGX_HTTP_BAD_GATEWAY));
    mrb_define_const(mrb, class, "NGX_HTTP_SERVICE_UNAVAILABLE", mrb_fixnum_value(NGX_HTTP_SERVICE_UNAVAILABLE));
    mrb_define_const(mrb, class, "NGX_HTTP_GATEWAY_TIME_OUT", mrb_fixnum_value(NGX_HTTP_GATEWAY_TIME_OUT));
    mrb_define_const(mrb, class, "NGX_HTTP_INSUFFICIENT_STORAGE", mrb_fixnum_value(NGX_HTTP_INSUFFICIENT_STORAGE));
    mrb_define_class_method(mrb, class, "rputs", ngx_mrb_rputs, ARGS_ANY());
    mrb_define_class_method(mrb, class, "send_header", ngx_mrb_send_header, ARGS_ANY());

    class_request = mrb_define_class_under(mrb, class, "Request", mrb->object_class);
    mrb_define_method(mrb, class_request, "content_type=", ngx_mrb_set_content_type, ARGS_ANY());
    mrb_define_method(mrb, class_request, "content_type", ngx_mrb_get_content_type, ARGS_NONE());
    mrb_define_method(mrb, class_request, "uri", ngx_mrb_get_request_uri, ARGS_NONE());

    return NGX_OK;
}

static mrb_value ngx_mrb_send_header(mrb_state *mrb, mrb_value self)
{
    ngx_http_request_t *r = ngx_mrb_get_request();

    mrb_int status = NGX_HTTP_OK;
    mrb_get_args(mrb, "i", &status);

    r->headers_out.status = status;
    ngx_http_send_header(r);

    return self;
}

static mrb_value ngx_mrb_rputs(mrb_state *mrb, mrb_value self)
{
    mrb_value msg;
    ngx_buf_t *b;
    ngx_chain_t out;
    u_char *str;

    ngx_http_request_t *r = ngx_mrb_get_request();

    b = ngx_pcalloc(r->pool, sizeof(ngx_buf_t));

    out.buf = b;
    out.next = NULL;

    mrb_get_args(mrb, "o", &msg);

    if (mrb_type(msg) != MRB_TT_STRING)
        return self;

    str         = (u_char *)RSTRING_PTR(msg);
    b->pos      = str;
    b->last     = str + strlen((char *)str);
    b->memory   = 1;
    b->last_buf = 1;

    r->headers_out.status = NGX_HTTP_OK;
    r->headers_out.content_length_n = strlen((char *)str);
    r->headers_out.content_type.len = sizeof("text/html") - 1;
    r->headers_out.content_type.data = (u_char *)"text/html";

    ngx_http_send_header(r);
    ngx_http_output_filter(r, &out);

    return self;
}

static mrb_value ngx_mrb_get_content_type(mrb_state *mrb, mrb_value self) 
{
    ngx_http_request_t *r = ngx_mrb_get_request();
    u_char *val = ngx_pstrdup(r->pool, &r->headers_out.content_type);
    return mrb_str_new(mrb, (char *)val, strlen((char *)val));
}

static mrb_value ngx_mrb_set_content_type(mrb_state *mrb, mrb_value self) 
{
    mrb_value arg;
    u_char *str;

    ngx_http_request_t *r = ngx_mrb_get_request();
    mrb_get_args(mrb, "o", &arg);
    str = (u_char *)RSTRING_PTR(arg);

    ngx_str_set(&r->headers_out.content_type, str);

    return self;
}

static mrb_value ngx_mrb_get_request_uri(mrb_state *mrb, mrb_value str)
{
    ngx_http_request_t *r = ngx_mrb_get_request();
    u_char *val = ngx_pstrdup(r->pool, &r->uri);
    return mrb_str_new(mrb, (char *)val, strlen((char *)val));
}
