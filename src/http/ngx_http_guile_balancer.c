/*
 * Copyright (C) lkpdn
 */

#include <stdio.h>
#include <libguile.h>
#include <ndk.h>

#include "../guile.h"
#include "ngx_http_guile_common.h"


typedef struct {
} ngx_http_guile_srv_conf_t;


typedef struct {
    /* the round robin data must be first */
    ngx_http_upstream_rr_peer_data_t rrp;
    ngx_http_guile_srv_conf_t        *conf;
    ngx_http_request_t               *request;

    int                              last_peer_state;
} ngx_http_guile_balancer_peer_data_t;


static ngx_int_t ngx_http_guile_balancer_init(ngx_conf_t *cf,
    ngx_http_upstream_srv_conf_t *us);
static ngx_int_t ngx_http_guile_balancer_init_peer(ngx_http_request_t *r,
    ngx_http_upstream_srv_conf_t *us);
static ngx_int_t ngx_http_guile_balancer_get_peer(ngx_peer_connection_t *pc,
    void *data);
static void ngx_http_guile_balancer_free_peer(ngx_peer_connection_t *pc,
    void *data, ngx_uint_t state);


char *
ngx_http_guile_balancer_by_guile(ngx_conf_t *cf, ngx_command_t *cmd,
    void *conf)
{
    ngx_str_t                    *value;
    ngx_http_upstream_srv_conf_t *uscf;

    uscf = ngx_http_conf_get_module_srv_conf(cf, ngx_http_upstream_module);

    value = cf->args->elts;

    if (value[1].len == 0) {
        ngx_conf_log_error(NGX_LOG_ERR, cf, 0,
                           "no runnable .scm code");
        return NGX_CONF_ERROR;
    }

    scm_with_guile(ngx_http_guile_spawn_thread, value[1].data);

    if (uscf->peer.init_upstream) {
        ngx_conf_log_error(NGX_LOG_WARN, cf, 0,
                           "load balancing method redefined");
    }

    uscf->peer.init_upstream = ngx_http_guile_balancer_init;

    uscf->flags = NGX_HTTP_UPSTREAM_CREATE
                  |NGX_HTTP_UPSTREAM_WEIGHT
                  |NGX_HTTP_UPSTREAM_MAX_FAILS
                  |NGX_HTTP_UPSTREAM_FAIL_TIMEOUT
                  |NGX_HTTP_UPSTREAM_DOWN;

    return NGX_CONF_OK;
}


static ngx_int_t
ngx_http_guile_balancer_init(ngx_conf_t *cf, ngx_http_upstream_srv_conf_t *us)
{
    if (ngx_http_upstream_init_round_robin(cf, us) != NGX_OK) {
        return NGX_ERROR;
    }

    us->peer.init = ngx_http_guile_balancer_init_peer;

    return NGX_OK;
}


static ngx_int_t
ngx_http_guile_balancer_init_peer(ngx_http_request_t *r,
    ngx_http_upstream_srv_conf_t *us)
{
    ngx_http_guile_srv_conf_t           *gsc;
    ngx_http_guile_balancer_peer_data_t *bp;

    bp = ngx_palloc(r->pool, sizeof(ngx_http_guile_balancer_peer_data_t));
    if (bp == NULL) {
        return NGX_ERROR;
    }

    r->upstream->peer.data = &bp->rrp;

    if (ngx_http_upstream_init_round_robin_peer(r, us) != NGX_OK) {
        return NGX_ERROR;
    }

    r->upstream->peer.get = ngx_http_guile_balancer_get_peer;
    r->upstream->peer.free = ngx_http_guile_balancer_free_peer;

    gsc = ngx_http_conf_upstream_srv_conf(us, ngx_http_guile_module);

    bp->conf = gsc;
    bp->request = r;

    return NGX_OK;
}


static ngx_int_t
ngx_http_guile_balancer_get_peer(ngx_peer_connection_t *pc, void *data)
{
    u_int                               idx;
    ngx_buf_t                           *buf;
    ngx_http_upstream_rr_peer_t         *peer;
    ngx_http_upstream_rr_peers_t        *peers;
    ngx_http_guile_balancer_peer_data_t *bp = data;

    peers = bp->rrp.peers;

    buf = bp->request->connection->buffer;

    call_args_t args = {
        .procedure = "balancer",
        .arg       = buf->start,
        .result    = 0,
    };

    scm_with_guile(ngx_http_guile_call, &args);

    idx = (u_int) args.result;
    peer = peers->peer;
    while (idx--) {
        peer = peer->next;
        if (!peer) {
            break;
        }
    }

    if (peer) {
        pc->sockaddr = peer->sockaddr;
        pc->socklen = peer->socklen;
        pc->name = &peer->name;
        return NGX_OK;
    }

    return ngx_http_upstream_get_round_robin_peer(pc, &bp->rrp);
}

static void
ngx_http_guile_balancer_free_peer(ngx_peer_connection_t *pc, void *data,
    ngx_uint_t state)
{
    ngx_http_guile_balancer_peer_data_t  *bp = data;

    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, pc->log, 0,
                   "guile balancer free peer, tries: %ui", pc->tries);

    bp->last_peer_state = (int) state;

    if (pc->tries) {
        pc->tries--;
    }

    return ;
}
