/*
 * Copyright (C) lkpdn
 */

#include <stdio.h>
#include <libguile.h>
#include <ndk.h>

#include "guile.h"


SCM scm_raw_buf_type;


static SCM
ngx_http_guile_load_file(void *data)
{
    scm_c_primitive_load((const char *)data);
    return SCM_UNSPECIFIED;
}


static SCM
scm_raw_buf_p(SCM obj) {
    return SCM_IS_A_P(obj, scm_raw_buf_type)? SCM_BOOL_T: SCM_BOOL_F;
}


static void
finalize_scm_raw_buf(SCM raw_buf) {
    /* empty */
}


static SCM
ngx_http_guile_define_foreign_types()
{
    SCM name, slots;
    scm_t_struct_finalize finalizer;

    /* raw_buf */
    name = scm_from_utf8_symbol("raw_buf");
    slots = scm_list_2(scm_from_utf8_symbol("len"),
                       scm_from_utf8_symbol("buf"));
    finalizer = finalize_scm_raw_buf;
    scm_raw_buf_type = scm_make_foreign_object_type(name, slots, finalizer);

    scm_c_define_gsubr("raw_buf?", 1, 0, 0, scm_raw_buf_p);

    return SCM_UNSPECIFIED;
}


void *
ngx_http_guile_spawn_thread(void *data)
{
    SCM th;

    th = scm_spawn_thread(ngx_http_guile_load_file, data, NULL, NULL);
    scm_join_thread(th);
    th = scm_spawn_thread(ngx_http_guile_define_foreign_types, NULL, NULL, NULL);
    scm_join_thread(th);
    return NULL;
}


void *
ngx_http_guile_call(void *data)
{
    call_args_t *args = (call_args_t *)data;
    SCM func, res;

    if (ngx_strcmp(args->procedure, "preaccess") == 0 ||
        ngx_strcmp(args->procedure, "access") == 0 ||
        ngx_strcmp(args->procedure, "balancer") == 0) {
        scm_make_foreign_object_2(scm_raw_buf_type, (void *)args->raw.len,
                                  (void *)args->raw.buffer);
    }

    func = scm_variable_ref(scm_c_lookup(args->procedure));
    res = scm_call_0(func);
    args->result = scm_is_null(res) ? 0 : scm_to_uint(res);

    return NULL;
}
