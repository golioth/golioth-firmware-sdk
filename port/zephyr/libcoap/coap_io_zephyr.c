/*
 * Copyright (C) 2012,2014 Olaf Bergmann <bergmann@tzi.org>
 *               2014 chrysn <chrysn@fsfe.org>
 *               2022-2023 Jon Shallow <supjps-libcoap@jpshallow.com>
 *               2023 Golioth, Inc.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * This file is part of the CoAP library libcoap. Please see
 * README for terms of use.
 */

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(coap_io_zephyr);

#include "coap3/coap_internal.h"

#include "coap_zephyr.h"

/*
 * coap_io_prepare_io() copy pasted from libcoap coap_io.c
 *
 * return  0 No i/o pending
 *       +ve millisecs to next i/o activity
 */
unsigned int coap_io_prepare_io(
        coap_context_t* ctx,
        coap_socket_t* sockets[],
        unsigned int max_sockets,
        unsigned int* num_sockets,
        coap_tick_t now) {
    coap_queue_t* nextpdu;
    coap_session_t *s, *rtmp;
    coap_tick_t timeout = 0;
    coap_tick_t s_timeout;

    *num_sockets = 0;

#if COAP_ASYNC_SUPPORT
    /* Check to see if we need to send off any Async requests */
    timeout = coap_check_async(ctx, now);
#endif /* COAP_ASYNC_SUPPORT */

    /* Check to see if we need to send off any retransmit request */
    nextpdu = coap_peek_next(ctx);
    while (nextpdu && now >= ctx->sendqueue_basetime
           && nextpdu->t <= now - ctx->sendqueue_basetime) {
        coap_retransmit(ctx, coap_pop_next(ctx));
        nextpdu = coap_peek_next(ctx);
    }
    if (nextpdu && (timeout == 0 || nextpdu->t - (now - ctx->sendqueue_basetime) < timeout))
        timeout = nextpdu->t - (now - ctx->sendqueue_basetime);

    SESSIONS_ITER_SAFE(ctx->sessions, s, rtmp) {
        if (s->type == COAP_SESSION_TYPE_CLIENT && s->state == COAP_SESSION_STATE_ESTABLISHED
            && ctx->ping_timeout > 0) {
            if (s->last_rx_tx + ctx->ping_timeout * COAP_TICKS_PER_SECOND <= now) {
                if ((s->last_ping > 0 && s->last_pong < s->last_ping)
                    || ((s->last_ping_mid = coap_session_send_ping(s)) == COAP_INVALID_MID)) {
                    /* Make sure the session object is not deleted in the callback */
                    coap_session_reference(s);
                    coap_session_disconnected(s, COAP_NACK_NOT_DELIVERABLE);
                    coap_session_release(s);
                    continue;
                }
                s->last_rx_tx = now;
                s->last_ping = now;
            }
            s_timeout = (s->last_rx_tx + ctx->ping_timeout * COAP_TICKS_PER_SECOND) - now;
            if (timeout == 0 || s_timeout < timeout)
                timeout = s_timeout;
        }

        /* Make sure the session object is not deleted in any callbacks */
        coap_session_reference(s);
        /* Check any DTLS timeouts and expire if appropriate */
        if (s->state == COAP_SESSION_STATE_HANDSHAKE && s->proto == COAP_PROTO_DTLS && s->tls) {
            if (coap_dtls_zephyr_connect(s))
                goto release_2;
        }

        /* Check if any client large receives have timed out */
        if (s->lg_crcv) {
            if (coap_block_check_lg_crcv_timeouts(s, now, &s_timeout)) {
                if (timeout == 0 || s_timeout < timeout)
                    timeout = s_timeout;
            }
        }
        /* Check if any client large sending have timed out */
        if (s->lg_xmit) {
            if (coap_block_check_lg_xmit_timeouts(s, now, &s_timeout)) {
                if (timeout == 0 || s_timeout < timeout)
                    timeout = s_timeout;
            }
        }

        assert(s->ref > 1);
        if (s->sock.flags
            & (COAP_SOCKET_WANT_READ | COAP_SOCKET_WANT_WRITE | COAP_SOCKET_WANT_CONNECT)) {
            if (*num_sockets < max_sockets)
                sockets[(*num_sockets)++] = &s->sock;
        }

    release_2:
        coap_session_release(s);
    }

    return (unsigned int)((timeout * 1000 + COAP_TICKS_PER_SECOND - 1) / COAP_TICKS_PER_SECOND);
}

int coap_io_process_with_fds(coap_context_t *ctx, uint32_t timeout_ms,
                             int nfds, fd_set *readfds, fd_set *writefds,
                             fd_set *exceptfds) {
    struct zsock_pollfd fds[ARRAY_SIZE(ctx->sockets) + CONFIG_GOLIOTH_LIBCOAP_EXTRA_FDS_MAX] = {};
    unsigned int num_pollfds = 0;
    coap_tick_t before, now;
    unsigned int timeout;
    int timeout_poll;
    int ret;

    coap_ticks(&before);

    timeout = coap_io_prepare_io(
            ctx,
            ctx->sockets,
            ARRAY_SIZE(ctx->sockets),
            &ctx->num_sockets,
            before);

    if (timeout == 0 || timeout_ms < timeout) {
        timeout = timeout_ms;
    }

    if (timeout == COAP_IO_NO_WAIT) {
        timeout_poll = 0;
    } else if (timeout == COAP_IO_WAIT) {
        timeout_poll = -1;
    } else {
        timeout_poll = timeout;
    }

    for (unsigned int i = 0; i < ctx->num_sockets; i++) {
        fds[i].fd = ctx->sockets[i]->fd;

        if (ctx->sockets[i]->flags & COAP_SOCKET_WANT_READ) {
            fds[i].events |= ZSOCK_POLLIN;
        }

        if (ctx->sockets[i]->flags & COAP_SOCKET_WANT_WRITE) {
            fds[i].events |= ZSOCK_POLLOUT;
        }

        LOG_DBG("fds[%d].fd %d .events %d", i, fds[i].fd, fds[i].events);
    }

    num_pollfds = ctx->num_sockets;

    for (unsigned int fd = 0; fd < nfds; fd++) {
        int events = 0;

        if (readfds) {
            events |= FD_ISSET(fd, readfds) ? ZSOCK_POLLIN : 0;
            FD_CLR(fd, readfds);
        }

        if (writefds) {
            events |= FD_ISSET(fd, writefds) ? ZSOCK_POLLOUT : 0;
            FD_CLR(fd, writefds);
        }

        if (!events) {
            continue;
        }

        if (num_pollfds >= ARRAY_SIZE(fds)) {
            LOG_WRN("Not enough fds");
            errno = ENOMEM;
            return -1;
        }

        fds[num_pollfds].fd = fd;
        fds[num_pollfds].events = events;

        num_pollfds++;
    }

    ret = zsock_poll(fds, num_pollfds, timeout_poll);

    if (ret < 0) {
        LOG_WRN("%s", strerror(errno));
        return -1;
    }

    if (ret > 0) {
        for (unsigned int i = 0; i < ctx->num_sockets; i++) {
            LOG_DBG("fds[%d].fd %d .revents %d", i, fds[i].fd, fds[i].revents);

            if ((ctx->sockets[i]->flags & COAP_SOCKET_WANT_READ)
                && (fds[i].revents & ZSOCK_POLLIN)) {
                ctx->sockets[i]->flags |= COAP_SOCKET_CAN_READ;
            }

            if ((ctx->sockets[i]->flags & COAP_SOCKET_WANT_WRITE)
                && (fds[i].revents & ZSOCK_POLLOUT)) {
                ctx->sockets[i]->flags |= COAP_SOCKET_CAN_WRITE;
            }
        }

        for (unsigned int i = ctx->num_sockets; i < num_pollfds; i++) {
            if (readfds && fds[i].revents & ZSOCK_POLLIN) {
                FD_SET(fds[i].fd, readfds);
            }

            if (writefds && fds[i].revents & ZSOCK_POLLOUT) {
                FD_SET(fds[i].fd, writefds);
            }
        }
    }

    coap_ticks(&now);
    coap_io_do_io(ctx, now);

    return 0;
}

int coap_io_process(coap_context_t* ctx, uint32_t timeout_ms) {
    return coap_io_process_with_fds(ctx, timeout_ms, 0, NULL, NULL, NULL);
}

void coap_packet_get_memmapped(coap_packet_t* packet, unsigned char** address, size_t* length) {
    *address = packet->payload;
    *length = packet->length;
}

ssize_t coap_socket_send(
        coap_socket_t* sock,
        const coap_session_t* session,
        const uint8_t* data,
        size_t data_len) {
    LOG_DBG("coap_socket_send()");

    return zsock_send(sock->fd, data, data_len, 0);
}

int coap_socket_connect_udp(
        coap_socket_t* sock,
        const coap_address_t* local_if,
        const coap_address_t* server,
        int default_port,
        coap_address_t* local_addr,
        coap_address_t* remote_addr) {
    coap_session_t* session = sock->session;
    coap_address_t connect_addr;
    int ret;

    LOG_DBG("coap_socket_connect_udp()");

    coap_address_copy(&connect_addr, server);

    sock->flags &= ~(COAP_SOCKET_CONNECTED | COAP_SOCKET_MULTICAST);
    sock->fd = zsock_socket(connect_addr.addr.sa.sa_family, SOCK_DGRAM, IPPROTO_DTLS_1_2);
    if (sock->fd < 0) {
        goto close_socket;
    }

    // If Connection IDs are enabled, set socket option to send CIDs, but not require that the
    // server sends one in return.
#ifdef CONFIG_GOLIOTH_USE_CONNECTION_ID
    int enabled = 1;
    ret = zsock_setsockopt(sock->fd, SOL_TLS, TLS_DTLS_CID, &enabled, sizeof(enabled));
    if (ret < 0) {
        goto close_socket;
    }
#endif /* CONFIG_GOLIOTH_USE_CONNECTION_ID */

    if (session->proto == COAP_PROTO_UDP) {
        ret = zsock_connect(sock->fd, &connect_addr.addr.sa, connect_addr.size);
        if (ret < 0) {
            goto close_socket;
        }
    }

    /*
     * This is a lie in case of DTLS sockets, as the connection happens
     * inside DTLS layer.
     *
     * TODO: connect to UDP first and then inject UDP socket into Zephyr
     * DTLS layer (see Zephyr Issue [1]).
     *
     * [1] https://github.com/zephyrproject-rtos/zephyr/issues/47341#issuecomment-1177506113
     */
    sock->flags |= COAP_SOCKET_CONNECTED;

    return 1;

close_socket:
    coap_socket_close(sock);

    return 0;
}

ssize_t coap_socket_write(coap_socket_t* sock, const uint8_t* data, size_t data_len) {
    (void)sock;
    (void)data;
    (void)data_len;
    return -1;
}

ssize_t coap_socket_read(coap_socket_t* sock, uint8_t* data, size_t data_len) {
    (void)sock;
    (void)data;
    (void)data_len;

    return -1;
}

void coap_socket_close(coap_socket_t* sock) {
    zsock_close(sock->fd);
    sock->fd = -1;
}

ssize_t coap_socket_recv(coap_socket_t* sock, coap_packet_t* packet) {
    ssize_t ret;

    if ((sock->flags & COAP_SOCKET_CAN_READ) == 0) {
        LOG_DBG("coap_socket_recv: COAP_SOCKET_CAN_READ not set");
        return -1;
    }

    /* clear has-data flag */
    sock->flags &= ~COAP_SOCKET_CAN_READ;

    if (!(sock->flags & COAP_SOCKET_CONNECTED)) {
        LOG_DBG("coap_socket_recv: !COAP_SOCKET_CONNECTED");
        return -1;
    }

    ret = zsock_recv(sock->fd, packet->payload, COAP_RXBUFFER_SIZE, 0);
    if (ret < 0) {
        if (errno == ECONNREFUSED || errno == EHOSTUNREACH) {
            /* client-side ICMP destination unreachable, ignore it */
            LOG_WRN("%s: coap_socket_recv: ICMP: %s",
                    sock->session ? coap_session_str(sock->session) : "",
                    strerror(errno));
            return -2;
        }

        LOG_WRN("%s: coap_socket_recv: %s",
                sock->session ? coap_session_str(sock->session) : "",
                strerror(errno));

        goto error;
    }

    if (ret > 0) {
        packet->length = ret;
    }

    if (ret >= 0) {
        return ret;
    }

error:
    return -1;
}

const char *
coap_socket_format_errno(int error) {
  return strerror(error);
}
const char *
coap_socket_strerror(void) {
  return coap_socket_format_errno(errno);
}
