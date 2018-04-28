/*
 * Copyright (C) lkpdn
 */

#include <stdio.h>
#include <libguile.h>

#include "guile.h"


static SCM
ngx_http_guile_load_file(void *data)
{
    scm_c_primitive_load((const char *)data);
    return SCM_UNSPECIFIED;
}


void *
ngx_http_guile_spawn_thread(void *data)
{
    SCM th;

    th = scm_spawn_thread(ngx_http_guile_load_file, data, NULL, NULL);
    scm_join_thread(th);
    return NULL;
}


void *
ngx_http_guile_call(void *data)
{
    call_args_t *args = (call_args_t *)data;
    SCM func, res;

    func = scm_variable_ref(scm_c_lookup(args->procedure));
    res = scm_call_1(func, scm_from_uchar(*(u_char *)args->arg));
    args->result = scm_is_null(res) ? 0 : scm_to_uint(res);

    return NULL;
}
