/*
 * Copyright (C) lkpdn
 */

#include <stdio.h>
#include <libguile.h>
#include <ndk.h>

#include "../guile.h"


typedef struct {
    unsigned required_preaccess:1;
    unsigned required_access:1;
} ngx_http_guile_main_conf_t;


static void * ngx_http_guile_create_main_conf(ngx_conf_t *cf);
static char * ngx_http_guile_preaccess_by_guile(ngx_conf_t *cf,
    ngx_command_t *cmd, void *conf);
static char * ngx_http_guile_access_by_guile(ngx_conf_t *cf,
    ngx_command_t *cmd, void *conf);
static ngx_int_t ngx_http_guile_init(ngx_conf_t *cf);


static ngx_command_t  ngx_http_guile_commands[] = {

    { ngx_string("preaccess_by_guile_file"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_HTTP_LIF_CONF
                        |NGX_CONF_TAKE1,
      ngx_http_guile_preaccess_by_guile,
      0,
      0,
      NULL },

    { ngx_string("access_by_guile_file"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_HTTP_LIF_CONF
                        |NGX_CONF_TAKE1,
      ngx_http_guile_access_by_guile,
      0,
      0,
      NULL },

    ngx_null_command
};


static ngx_http_module_t  ngx_http_guile_module_ctx = {
    NULL,                            /* preconfiguration */
    ngx_http_guile_init,             /* postconfiguration */

    ngx_http_guile_create_main_conf, /* create main configuration */
    NULL,                            /* init main configuration */

    NULL,                            /* create server configuration */
    NULL,                            /* merge server configuration */

    NULL,                            /* create location configuration */
    NULL                             /* merge location configuration */
};


ngx_module_t  ngx_http_guile_module = {
    NGX_MODULE_V1,
    &ngx_http_guile_module_ctx,  /* module context */
    ngx_http_guile_commands,     /* module directives */
    NGX_HTTP_MODULE,             /* module type */
    NULL,                        /* init master */
    NULL,                        /* init module */
    NULL,                        /* init process */
    NULL,                        /* init thread */
    NULL,                        /* exit thread */
    NULL,                        /* exit process */
    NULL,                        /* exit master */
    NGX_MODULE_V1_PADDING
};


static void *
ngx_http_guile_create_main_conf(ngx_conf_t *cf)
{
    ngx_http_guile_main_conf_t  *gmcf;

    gmcf = ngx_pcalloc(cf->pool, sizeof(ngx_http_guile_main_conf_t));
    if (gmcf == NULL) {
        return NULL;
    }

    return gmcf;
}


static char *
ngx_http_guile_preaccess_by_guile(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_str_t *value;
    ngx_http_guile_main_conf_t  *gmcf;

    gmcf = ngx_http_conf_get_module_main_conf(cf, ngx_http_guile_module);

    value = cf->args->elts;

    if (value[1].len == 0) {
        ngx_conf_log_error(NGX_LOG_ERR, cf, 0,
                           "no runnable .scm code");
        return NGX_CONF_ERROR;
    }

    scm_with_guile(ngx_http_guile_spawn_thread, value[1].data);

    gmcf->required_preaccess = 1;
    return NGX_CONF_OK;
}


static char *
ngx_http_guile_access_by_guile(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_str_t *value;
    ngx_http_guile_main_conf_t  *gmcf;

    gmcf = ngx_http_conf_get_module_main_conf(cf, ngx_http_guile_module);

    value = cf->args->elts;

    if (value[1].len == 0) {
        ngx_conf_log_error(NGX_LOG_ERR, cf, 0,
                           "no runnable .scm code");
        return NGX_CONF_ERROR;
    }

    scm_with_guile(ngx_http_guile_spawn_thread, value[1].data);

    gmcf->required_access = 1;
    return NGX_CONF_OK;
}


static ngx_int_t
ngx_http_guile_preaccess_handler(ngx_http_request_t *r)
{
    ngx_buf_t                   *buf;
    ngx_http_guile_main_conf_t  *gmcf;

    gmcf = ngx_http_get_module_main_conf(r, ngx_http_guile_module);

    buf = r->connection->buffer;

    call_args_t args = {
        .procedure = "preaccess",
        .arg       = buf->start,
        .result    = 0,
    };

    scm_with_guile(ngx_http_guile_call, &args);

    if (args.result) {
        return NGX_DECLINED;
    }

    return NGX_OK;
}


static ngx_int_t
ngx_http_guile_access_handler(ngx_http_request_t *r)
{
    ngx_buf_t                   *buf;
    ngx_http_guile_main_conf_t  *gmcf;

    gmcf = ngx_http_get_module_main_conf(r, ngx_http_guile_module);

    buf = r->connection->buffer;

    call_args_t args = {
        .procedure = "access",
        .arg       = buf->start,
        .result    = 0,
    };

    scm_with_guile(ngx_http_guile_call, &args);

    if (args.result) {
        return NGX_DECLINED;
    }

    return NGX_OK;
}


static ngx_int_t
ngx_http_guile_init(ngx_conf_t *cf)
{
    ngx_http_handler_pt         *h;
    ngx_http_core_main_conf_t   *cmcf;
    ngx_http_guile_main_conf_t  *gmcf;

    cmcf = ngx_http_conf_get_module_main_conf(cf, ngx_http_core_module);
    gmcf = ngx_http_conf_get_module_main_conf(cf, ngx_http_guile_module);

    if (gmcf->required_preaccess) {
        h = ngx_array_push(&cmcf->phases[NGX_HTTP_PREACCESS_PHASE].handlers);
        if (h == NULL) {
            return NGX_ERROR;
        }

        *h = ngx_http_guile_preaccess_handler;
    }

    if (gmcf->required_access) {
        h = ngx_array_push(&cmcf->phases[NGX_HTTP_ACCESS_PHASE].handlers);
        if (h == NULL) {
            return NGX_ERROR;
        }

        *h = ngx_http_guile_access_handler;
    }

    return NGX_OK;
}
