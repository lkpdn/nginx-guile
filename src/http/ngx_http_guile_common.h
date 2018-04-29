/*
 * Copyright (C) lkpdn
 */

#ifndef _NGX_HTTP_GUILE_COMMON_H_INCLUDED_
#define _NGX_HTTP_GUILE_COMMON_H_INCLUDED_


extern ngx_module_t ngx_http_guile_module;

typedef struct {
    unsigned required_preaccess:1;
    unsigned required_access:1;
} ngx_http_guile_main_conf_t;

#endif /* _NGX_HTTP_GUILE_COMMON_H_INCLUDED_ */
