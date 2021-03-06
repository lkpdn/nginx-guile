/*
 * Copyright (C) lkpdn
 */

#ifndef _NGINX_GUILE_H_INCLUDED_
#define _NGINX_GUILE_H_INCLUDED_


typedef struct {
    const char *procedure;
    union {
        struct {
            size_t len;
            u_char *buffer;
        } raw;
    };
    u_char result;
} call_args_t;


void * ngx_http_guile_spawn_thread(void *data);
void * ngx_http_guile_call(void *data);

#endif /* _NGINX_GUILE_H_INCLUDED_ */
