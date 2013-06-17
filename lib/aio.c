/** \file lib/aio.c
  \class aio
  asynch event-driven non-blocking IO, via libuv, on files, network streams, processes.
  \see http://nikhilm.github.io/uvbook/basics.html

  For POSIX unbuffered fileio \see file.c open,read,write,seek calls on fd
  and lib/buffile.c for buffered IO.

 (c) 2013 perl11.org */
#include <stdio.h>
#include <stdlib.h>
#include <uv.h>
#include "p2.h"

PN aio_vt, aio_tcp_vt, aio_udp_vt, aio_loop_vt, aio_tty_vt, aio_pipe_vt, aio_poll_vt, aio_prepare_vt,
  aio_idle_vt, aio_async_vt, aio_timer_vt, aio_fs_poll_vt, aio_signal_vt, aio_fs_event_vt, aio_mutex_vt,
  aio_rwlock_vt, aio_sem_vt, aio_cond_vt, aio_barrier_vt, aio_check_vt,
  aio_handle_vt, aio_process_vt, aio_stream_vt, aio_req_vt, aio_connect_vt, aio_write_vt, aio_shutdown_vt,
  aio_udp_send_vt, aio_fs_vt, aio_work_vt, aio_getaddrinfo_vt, aio_cpu_info_vt, aio_interface_address_vt;

#define DEF_AIO_REQ_WRAP(T,H) \
struct aio_##T##_s { \
  struct uv_##H##_s r;   \
  Potion *P;             \
  PN cl;                 \
  uv_##T##_cb cb;        \
} aio_##T##_t
#define DEF_AIO_HANDLE_WRAP(T) \
struct aio_##T##_s {     \
  uv_##T##_t h;          \
  Potion *P;             \
  PN cl;                 \
} aio_##T##_t
DEF_AIO_REQ_WRAP(write,write);
DEF_AIO_REQ_WRAP(connect,connect);
DEF_AIO_REQ_WRAP(shutdown,shutdown);
DEF_AIO_REQ_WRAP(connection,stream);
DEF_AIO_REQ_WRAP(close,handle);
DEF_AIO_REQ_WRAP(poll,poll);
DEF_AIO_REQ_WRAP(timer,timer);
DEF_AIO_REQ_WRAP(async,async);
DEF_AIO_REQ_WRAP(prepare,prepare);
DEF_AIO_REQ_WRAP(check,check);
DEF_AIO_REQ_WRAP(idle,idle);
DEF_AIO_REQ_WRAP(exit,process);
DEF_AIO_REQ_WRAP(walk,handle);
DEF_AIO_REQ_WRAP(fs,fs);
DEF_AIO_REQ_WRAP(work,work);
DEF_AIO_REQ_WRAP(after_work,work);
DEF_AIO_REQ_WRAP(getaddrinfo,getaddrinfo);
DEF_AIO_HANDLE_WRAP(handle);
DEF_AIO_HANDLE_WRAP(loop);
DEF_AIO_HANDLE_WRAP(fs_event);
DEF_AIO_HANDLE_WRAP(pipe);
DEF_AIO_HANDLE_WRAP(process);
DEF_AIO_HANDLE_WRAP(stream);
DEF_AIO_HANDLE_WRAP(tty);
DEF_AIO_HANDLE_WRAP(req);
DEF_AIO_HANDLE_WRAP(tcp);
DEF_AIO_HANDLE_WRAP(udp);
DEF_AIO_HANDLE_WRAP(udp_send);
DEF_AIO_HANDLE_WRAP(cpu_info);
DEF_AIO_HANDLE_WRAP(interface_address);
DEF_AIO_HANDLE_WRAP(sem);
DEF_AIO_HANDLE_WRAP(barrier);
DEF_AIO_HANDLE_WRAP(cond);
DEF_AIO_HANDLE_WRAP(mutex);
DEF_AIO_HANDLE_WRAP(rwlock);
//DEF_AIO_HANDLE_WRAP();
#undef DEF_AIO_REQ_WRAP
#undef DEF_AIO_HANDLE_WRAP

#define DEF_AIO_NEW(T)					\
  static PN aio_##T##_new(Potion *P, PN cl, PN self) {	\
    struct PNData *data = potion_data_alloc(P,		\
	sizeof(uv_##T##_t)+sizeof(Potion*)+sizeof(PN)+sizeof(void*)); \
    data->vt = aio_##T##_vt;					      \
    ((struct aio_##T##_s*)data)->P = P;			              \
    ((struct aio_##T##_s*)data)->cl = cl;			      \
    return (PN)data;						      \
  }
DEF_AIO_NEW(loop)
DEF_AIO_NEW(async)
DEF_AIO_NEW(fs_event)
DEF_AIO_NEW(handle)
DEF_AIO_NEW(pipe)
DEF_AIO_NEW(poll)
DEF_AIO_NEW(process)
DEF_AIO_NEW(stream)
DEF_AIO_NEW(tty)
DEF_AIO_NEW(req)
DEF_AIO_NEW(connect)
DEF_AIO_NEW(write)
DEF_AIO_NEW(shutdown)
DEF_AIO_NEW(udp_send)
DEF_AIO_NEW(fs)
DEF_AIO_NEW(work)
DEF_AIO_NEW(getaddrinfo)
DEF_AIO_NEW(cpu_info)
DEF_AIO_NEW(interface_address)
DEF_AIO_NEW(sem)
DEF_AIO_NEW(barrier)
#undef DEF_AIO_NEW

#define DEF_AIO_INIT(T) \
  static PN aio_##T##_new(Potion *P, PN cl, PN self) {                \
    struct PNData *data = potion_data_alloc(P,                        \
	sizeof(uv_##T##_t)+sizeof(Potion*)+sizeof(PN)+sizeof(void*)); \
    data->vt = aio_##T##_vt;                                          \
    ((struct aio_##T##_s*)data)->P = P;			              \
    ((struct aio_##T##_s*)data)->cl = cl;			      \
    return (PN)data;                                                  \
  }								      \
  static PN aio_##T##_init(Potion *P, PN cl, PN self) {               \
    int r;                                                            \
    uv_##T##_t *handle = (uv_##T##_t*)PN_DATA(self);                  \
    r = uv_##T##_init(handle);                                        \
    if (!r) return potion_io_error(P, _XSTR(t) "_init");              \
    return self;                                                      \
  }
#define DEF_AIO_INIT_LOOP(T) \
  static PN aio_##T##_new(Potion *P, PN cl, PN self) {                \
    struct PNData *data = potion_data_alloc(P,                        \
	sizeof(uv_##T##_t)+sizeof(Potion*)+sizeof(PN)+sizeof(void*)); \
    data->vt = aio_##T##_vt;                                          \
    return (PN)data;                                                  \
  }								      \
  static PN aio_##T##_init(Potion *P, PN cl, PN self, PN loop) {      \
    int r;                                                            \
    uv_##T##_t *handle = (uv_##T##_t*)PN_DATA(self);                  \
    if (!loop) loop = (PN)uv_default_loop();                          \
    else loop = (PN)PN_DATA(loop);                                    \
    r = uv_##T##_init((uv_loop_t*)loop, handle);                      \
    if (!r) return potion_io_error(P, _XSTR(t) "_init");              \
    return self;                                                      \
  }

/**\class aio_tcp \memberof aio_tcp
   \fn PN aio_tcp_init(PN loop)
   initialize aio_tcp
   \param loop   PNAioLoop to uv_loop_t*, defaults to uv_default_loop()
   \see http://nikhilm.github.io/uvbook/networking.html#tcp
   \returns self */
DEF_AIO_INIT_LOOP(tcp)
/**\class aio_udp \memberof aio_udp
   initialize aio_udp
   \fn PN aio_udp_init(PN loop)
   \param loop   PNAioLoop to uv_loop_t*, defaults to uv_default_loop()
   \see http://nikhilm.github.io/uvbook/networking.html#udp
   \returns self */
DEF_AIO_INIT_LOOP(udp)
DEF_AIO_INIT_LOOP(prepare)
DEF_AIO_INIT_LOOP(check)
DEF_AIO_INIT_LOOP(idle)
DEF_AIO_INIT_LOOP(timer)
DEF_AIO_INIT_LOOP(fs_poll)
DEF_AIO_INIT_LOOP(signal)
DEF_AIO_INIT(cond)
DEF_AIO_INIT(mutex)
DEF_AIO_INIT(rwlock)
#undef DEF_AIO_INIT

static PN aio_tty_init(Potion *P, PN cl, PN self, PN loop, PN file, PN readable) {
  int r;
  uv_tty_t *handle = (uv_tty_t*)PN_DATA(self);
  if (!loop) loop = (PN)uv_default_loop();
  else loop = (PN)PN_DATA(loop);
  r = uv_tty_init((uv_loop_t*)loop, handle, PN_NUM(PN_DATA(file)), PN_NUM(readable));
  if (!r) return potion_io_error(P, "tty_init");
  return self;
}
static PN aio_pipe_init(Potion *P, PN cl, PN self, PN loop, PN ipc) {
  int r;
  uv_pipe_t *handle = (uv_pipe_t*)PN_DATA(self);
  if (!loop) loop = (PN)uv_default_loop();
  else loop = (PN)PN_DATA(loop);
  r = uv_pipe_init((uv_loop_t*)loop, handle, PN_NUM(ipc));
  if (!r) return potion_io_error(P, "pipe_init");
  return self;
}
static PN aio_poll_init(Potion *P, PN cl, PN self, PN loop, PN fd) {
  int r;
  uv_poll_t *handle = (uv_poll_t*)PN_DATA(self);
  if (!loop) loop = (PN)uv_default_loop();
  else loop = (PN)PN_DATA(loop);
  r = uv_poll_init((uv_loop_t*)loop, handle, PN_NUM(fd));
  if (!r) return potion_io_error(P, "poll_init");
  return self;
}
static PN aio_barrier_init(Potion *P, PN cl, PN self, PN count) {
  int r;
  uv_barrier_t *handle = (uv_barrier_t*)PN_DATA(self);
  r = uv_barrier_init(handle, PN_NUM(count));
  if (!r) return potion_io_error(P, "barrier_init");
  return self;
}
static PN aio_sem_init(Potion *P, PN cl, PN self, PN value) {
  int r;
  uv_sem_t *handle = (uv_sem_t*)PN_DATA(self);
  r = uv_sem_init(handle, PN_NUM(value));
  if (!r) return potion_io_error(P, "sem_init");
  return self;
}
static PN aio_fs_event_init(Potion *P, PN cl, PN self, PN loop, PN filename, PN cb, PN flags) {
  int r;
  uv_fs_event_t *handle = (uv_fs_event_t*)PN_DATA(self);
  if (!loop) loop = (PN)uv_default_loop();
  else loop = (PN)PN_DATA(loop);
  r = uv_fs_event_init((uv_loop_t*)loop, handle, PN_STR_PTR(filename), (uv_fs_event_cb)PN_DATA(cb), PN_NUM(flags));
  if (!r) return potion_io_error(P, "fs_event_init");
  return self;
}
static PN aio_async_init(Potion *P, PN cl, PN self, PN loop, PN cb) {
  int r;
  uv_async_t *handle = (uv_async_t*)PN_DATA(self);
  if (!loop) loop = (PN)uv_default_loop();
  else loop = (PN)PN_DATA(loop);
  r = uv_async_init((uv_loop_t*)loop, handle, (uv_async_cb)PN_DATA(cb));
  if (!r) return potion_io_error(P, "async_init");
  return self;
}

//cb wrappers
#define DEF_AIO_CB(N,T)                     \
static void                                 \
aio_##N##_cb(uv_##T##_t* req, int status) { \
  struct aio_##T##_s* wrap = (struct aio_##T##_s*)req; \
  vPN(Closure) cb = PN_CLOSURE(wrap->cb);              \
  PN data = aio_##T##_new(wrap->P, wrap->cl, 0);       \
  memcpy(PN_DATA(data), req, sizeof(uv_##T##_t));      \
  if (cb) cb->method(wrap->P, wrap->cl, data, PN_NUM(status)); \
}
DEF_AIO_CB(write,write)
DEF_AIO_CB(connect,connect)
//DEF_AIO_CB(shutdown,shutdown)
//DEF_AIO_CB(connection,stream)
//DEF_AIO_CB(timer,timer)
//DEF_AIO_CB(async,async)

static PN
aio_tcp_open(Potion *P, PN cl, PN tcp, PN sock) {
  uv_tcp_t *handle = (uv_tcp_t*)PN_DATA(potion_fwd(tcp));
  return PN_NUM(uv_tcp_open(handle, PN_INT(sock)));
}
static PN
aio_tcp_nodelay(Potion *P, PN cl, PN tcp, PN enable) {
  uv_tcp_t *handle = (uv_tcp_t*)PN_DATA(potion_fwd(tcp));
  return PN_NUM(uv_tcp_nodelay(handle, PN_INT(enable)));
}
static PN
aio_tcp_keepalive(Potion *P, PN cl, PN tcp, PN enable, PN delay) {
  uv_tcp_t *handle = (uv_tcp_t*)PN_DATA(potion_fwd(tcp));
  return PN_NUM(uv_tcp_keepalive(handle, PN_INT(enable), PN_INT(delay)));
}
static PN
aio_tcp_simultaneous_accepts(Potion *P, PN cl, PN tcp, PN enable) {
  uv_tcp_t *handle = (uv_tcp_t*)PN_DATA(potion_fwd(tcp));
  return PN_NUM(uv_tcp_simultaneous_accepts(handle, PN_INT(enable)));
}
static PN
aio_tcp_bind(Potion *P, PN cl, PN tcp, PN addr, PN port) {
  uv_tcp_t *handle = (uv_tcp_t*)PN_DATA(potion_fwd(tcp));
  struct sockaddr_in ip4 = uv_ip4_addr(PN_STR_PTR(addr), PN_INT(port));
  int r = uv_tcp_bind(handle, ip4);
  return r ? PN_NIL : tcp;
}
static PN
aio_tcp_bind6(Potion *P, PN cl, PN tcp, PN addr, PN port) {
  uv_tcp_t *handle = (uv_tcp_t*)PN_DATA(potion_fwd(tcp));
  struct sockaddr_in6 ip6 = uv_ip6_addr(PN_STR_PTR(addr), PN_INT(port));
  int r = uv_tcp_bind6(handle, ip6);
  return r ? PN_NIL : tcp;
}
static PN
aio_tcp_getsockname(Potion *P, PN cl, PN tcp) {
  struct sockaddr sock; int len;
  uv_tcp_t *handle = (uv_tcp_t*)PN_DATA(potion_fwd(tcp));
  int r = uv_tcp_getsockname(handle, &sock, &len);
  return !r ? potion_str2(P, sock.sa_data, len) : PN_NIL;
}
static PN
aio_tcp_getpeername(Potion *P, PN cl, PN tcp) {
  struct sockaddr sock; int len;
  uv_tcp_t *handle = (uv_tcp_t*)PN_DATA(potion_fwd(tcp));
  int r = uv_tcp_getpeername(handle, &sock, &len);
  return !r ? potion_str2(P, sock.sa_data, len) : PN_NIL;
}
static PN
aio_tcp_connect(Potion *P, PN cl, PN tcp, PN req, PN addr, PN port, PN cb) {
  uv_tcp_t *handle = (uv_tcp_t*)PN_DATA(potion_fwd(tcp));
  uv_connect_t *request = (uv_connect_t*)PN_DATA(potion_fwd(req));
  uv_connect_cb connect_cb;
  struct sockaddr_in ip4 = uv_ip4_addr(PN_STR_PTR(addr), PN_INT(port));
  if (PN_IS_CLOSURE(cb)) { //register user-callback. set cb ptr in req
    request->cb = (uv_connect_cb)PN_CLOSURE(cb);
    connect_cb = (uv_connect_cb)aio_connect_cb;
  }
  else if (PN_IS_FFIPTR(cb)) connect_cb = (uv_connect_cb)cb; //c-level cb loaded via ffi
  else connect_cb = 0; //none
  return PN_NUM(uv_tcp_connect(request, handle, ip4, connect_cb));
}
static PN
aio_tcp_connect6(Potion *P, PN cl, PN tcp, PN req, PN addr, PN port, PN cb) {
  uv_tcp_t *handle = (uv_tcp_t*)PN_DATA(potion_fwd(tcp));
  uv_connect_t *request = (uv_connect_t*)PN_DATA(potion_fwd(req));
  uv_connect_cb connect_cb;
  struct sockaddr_in6 ip6 = uv_ip6_addr(PN_STR_PTR(addr), PN_INT(port));
  if (PN_IS_CLOSURE(cb)) { //register user-callback. set cb ptr in req
    request->cb = (uv_connect_cb)PN_CLOSURE(cb);
    connect_cb = (uv_connect_cb)aio_connect_cb;
  }
  else if (PN_IS_FFIPTR(cb)) connect_cb = (uv_connect_cb)cb; //c-level cb loaded via ffi
  else connect_cb = 0; //none
  return PN_NUM(uv_tcp_connect6(request, handle, ip6, connect_cb));
}
///\returns aio_udp or nil
static PN
aio_udp_open(Potion *P, PN cl, PN udp, PN sockfd) {
  uv_udp_t *handle = (uv_udp_t*)PN_DATA(potion_fwd(udp));
  int r = uv_udp_open(handle, PN_INT(sockfd));
  return r ? PN_NIL : udp;
}
static PN
aio_udp_bind(Potion *P, PN cl, PN udp, PN addr, PN port) {
  uv_udp_t *handle = (uv_udp_t*)PN_DATA(potion_fwd(udp));
  int flags = 0; //unused
  struct sockaddr_in ip4 = uv_ip4_addr(PN_STR_PTR(addr), PN_INT(port));
  int r = uv_udp_bind(handle, ip4, flags);
  return r ? PN_NIL : udp;
}
static PN
aio_udp_bind6(Potion *P, PN cl, PN udp, PN addr, PN port, PN flags) {
  uv_udp_t *handle = (uv_udp_t*)PN_DATA(potion_fwd(udp));
  struct sockaddr_in6 ip6 = uv_ip6_addr(PN_STR_PTR(addr), PN_INT(port));
  int r = uv_udp_bind6(handle, ip6, PN_INT(flags));
  return r ? PN_NIL : udp;
}
static PN
aio_udp_getsockname(Potion *P, PN cl, PN udp) {
  struct sockaddr sock; int len;
  uv_udp_t *handle = (uv_udp_t*)PN_DATA(potion_fwd(udp));
  int r = uv_udp_getsockname(handle, &sock, &len);
  return !r ? potion_str2(P, sock.sa_data, len) : PN_NIL;
}
static PN
aio_udp_set_membership(Potion *P, PN cl, PN udp, PN mcaddr, PN ifaddr, PN membership) {
  uv_udp_t *handle = (uv_udp_t*)PN_DATA(potion_fwd(udp));
  int r = uv_udp_set_membership(handle, PN_STR_PTR(mcaddr), PN_STR_PTR(ifaddr), PN_NUM(membership));
  return r ? PN_NIL : udp;
}
static PN
aio_udp_set_multicast_loop(Potion *P, PN cl, PN udp, PN on) {
  uv_udp_t *handle = (uv_udp_t*)PN_DATA(potion_fwd(udp));
  int r = uv_udp_set_multicast_loop(handle, PN_NUM(on)); //TODO boolean
  return r ? PN_NIL : udp;
}
static PN
aio_udp_set_multicast_ttl(Potion *P, PN cl, PN udp, PN on) {
  uv_udp_t *handle = (uv_udp_t*)PN_DATA(potion_fwd(udp));
  int r = uv_udp_set_multicast_ttl(handle, PN_NUM(on)); //TODO boolean
  return r ? PN_NIL : udp;
}
static PN
aio_udp_set_broadcast(Potion *P, PN cl, PN udp, PN on) {
  uv_udp_t *handle = (uv_udp_t*)PN_DATA(potion_fwd(udp));
  int r = uv_udp_set_broadcast(handle, PN_NUM(on)); //TODO boolean
  return r ? PN_NIL : udp;
}
static PN
aio_udp_set_ttl(Potion *P, PN cl, PN udp, PN ttl) {
  uv_udp_t *handle = (uv_udp_t*)PN_DATA(potion_fwd(udp));
  int r = uv_udp_set_ttl(handle, PN_NUM(ttl)); //TODO boolean
  return r ? PN_NIL : udp;
}
static PN
aio_udp_send(Potion *P, PN cl, PN udp, PN req, PN buf, PN bufcnt, PN addr, PN port, PN cb) {
  uv_udp_t *handle = (uv_udp_t*)PN_DATA(potion_fwd(udp));
  uv_udp_send_t *request = (uv_udp_send_t*)PN_DATA(potion_fwd(req));
  struct sockaddr_in ip4 = uv_ip4_addr(PN_STR_PTR(addr), PN_INT(port));
  uv_buf_t *bufs = (uv_buf_t*)PN_DATA(potion_fwd(buf));
  uv_udp_send_cb send_cb;
  if (PN_IS_CLOSURE(cb)) { //register user-callback. set cb ptr in req
    request->cb = (uv_udp_send_cb)PN_CLOSURE(cb);
    send_cb = (uv_udp_send_cb)NULL;//aio_send_udp_cb;
  }
  else if (PN_IS_FFIPTR(cb)) send_cb = (uv_udp_send_cb)cb; //c-level cb loaded via ffi
  else send_cb = 0; //none
  int r = uv_udp_send(request, handle, bufs, bufcnt, ip4, send_cb);
  return r ? PN_NIL : udp;
}
static PN
aio_udp_send6(Potion *P, PN cl, PN udp, PN req, PN buf, PN bufcnt, PN addr, PN port, PN cb) {
  uv_udp_t *handle = (uv_udp_t*)PN_DATA(potion_fwd(udp));
  uv_udp_send_t *request = (uv_udp_send_t*)PN_DATA(potion_fwd(req));
  struct sockaddr_in6 ip6 = uv_ip6_addr(PN_STR_PTR(addr), PN_INT(port));
  uv_buf_t *bufs = (uv_buf_t*)PN_DATA(potion_fwd(buf));
  uv_udp_send_cb send_cb;
  if (PN_IS_CLOSURE(cb)) { //register user-callback. set cb ptr in req
    request->cb = (uv_udp_send_cb)PN_CLOSURE(cb);
    send_cb = (uv_udp_send_cb)NULL;//aio_send_udp_cb;
  }
  else if (PN_IS_FFIPTR(cb)) send_cb = (uv_udp_send_cb)cb; //c-level cb loaded via ffi
  else send_cb = 0; //none
  int r = uv_udp_send6(request, handle, bufs, bufcnt, ip6, send_cb);
  return r ? PN_NIL : udp;
}

static uv_buf_t aio_alloc_cb(uv_handle_t* handle, size_t size) {
  struct aio_handle_s* wrap = (struct aio_handle_s*)handle;
  struct PNBytes *buf = potion_gc_alloc(wrap->P, PN_TBYTES, size);
  return (uv_buf_t)uv_buf_init(buf->chars, size);
}

static PN
aio_udp_recv_start(Potion *P, PN cl, PN udp, PN cb) {
  struct aio_udp_s *handle = (struct aio_udp_s*)PN_DATA(potion_fwd(udp));
  uv_udp_recv_cb recv_cb;
  if (PN_IS_CLOSURE(cb)) { //register user-callback. set cb ptr in req
    handle->h.recv_cb = (uv_udp_recv_cb)PN_CLOSURE(cb);
    recv_cb = (uv_udp_recv_cb)NULL;//aio_udp_recv_cb;
  }
  else if (PN_IS_FFIPTR(cb)) recv_cb = (uv_udp_recv_cb)cb; //c-level cb loaded via ffi
  else recv_cb = 0; //none
  int r = uv_udp_recv_start(&handle->h, aio_alloc_cb, recv_cb);
  return r ? PN_NIL : udp;
}
static PN
aio_udp_recv_stop(Potion *P, PN cl, PN udp) {
  uv_udp_t *handle = (uv_udp_t*)PN_DATA(potion_fwd(udp));
  int r = uv_udp_recv_stop(handle);
  return r ? PN_NIL : udp;
}

static PN
aio_pipe_open(Potion *P, PN cl, PN pipe, PN file) {
  uv_pipe_t *handle = (uv_pipe_t*)PN_DATA(potion_fwd(pipe));
  return PN_NUM(uv_pipe_open(handle, PN_INT(file)));
}
static PN
aio_pipe_bind(Potion *P, PN cl, PN pipe, PN name) {
  uv_pipe_t *handle = (uv_pipe_t*)PN_DATA(potion_fwd(pipe));
  return PN_NUM(uv_pipe_bind(handle, PN_STR_PTR(name)));
}
static PN
aio_pipe_connect(Potion *P, PN cl, PN pipe, PN req, PN name, PN cb) {
  uv_pipe_t *handle = (uv_pipe_t*)PN_DATA(potion_fwd(pipe));
  uv_connect_t *request = (uv_connect_t*)PN_DATA(potion_fwd(req));
  uv_connect_cb connect_cb;
  if (PN_IS_CLOSURE(cb)) { //register user-callback. set cb ptr in req
    request->cb = (uv_connect_cb)PN_CLOSURE(cb);
    connect_cb = (uv_connect_cb)aio_connect_cb;
  }
  else if (PN_IS_FFIPTR(cb)) connect_cb = (uv_connect_cb)cb; //c-level cb loaded via ffi
  else connect_cb = 0; //none
  uv_pipe_connect(request, handle, PN_STR_PTR(name), connect_cb);
  return pipe;
}

static PN
aio_write(Potion *P, PN cl, PN stream, PN req, PN buf, PN bufcnt, PN cb) {
  uv_stream_t *stm = (uv_stream_t*)PN_DATA(potion_fwd(stream));
  uv_write_t *request = (uv_write_t*)PN_DATA(potion_fwd(req));
  uv_buf_t *bufs = (uv_buf_t*)PN_DATA(potion_fwd(buf));
  uv_write_cb write_cb;
  if (PN_IS_CLOSURE(cb)) { //register user-callback. set cb ptr in req
    request->cb = (uv_write_cb)PN_CLOSURE(cb);
    write_cb = (uv_write_cb)aio_write_cb;
  }
  else if (PN_IS_FFIPTR(cb)) write_cb = (uv_write_cb)cb; //c-level cb loaded via ffi
  else write_cb = 0; //none
  return PN_NUM(uv_write(request, stm, bufs, PN_INT(bufcnt), write_cb));
}

/**\memberof aio
   allocate a req or handle struct and return a new \c aio_"type" object.
   \param type PNString
 */
static PN
aio_new(Potion *P, PN cl, PN self, PN type) {
#define DEF_AIO_OBJ(T)                                                  \
  if (type == PN_STR(_XSTR(T))) {                                       \
    struct PNData *data = potion_data_alloc(P, sizeof(uv_##T##_t));     \
    data->vt = aio_##T##_vt;                                            \
    return (PN)data;                                                    \
  } else

  DEF_AIO_OBJ(loop)
  DEF_AIO_OBJ(async)
  DEF_AIO_OBJ(check)
  DEF_AIO_OBJ(fs_event)
  DEF_AIO_OBJ(fs_poll)
  DEF_AIO_OBJ(handle)
  DEF_AIO_OBJ(idle)
  DEF_AIO_OBJ(pipe)
  DEF_AIO_OBJ(poll)
  DEF_AIO_OBJ(prepare)
  DEF_AIO_OBJ(process)
  DEF_AIO_OBJ(stream)
  DEF_AIO_OBJ(tcp)
  DEF_AIO_OBJ(timer)
  DEF_AIO_OBJ(tty)
  DEF_AIO_OBJ(udp)
  DEF_AIO_OBJ(signal)
  DEF_AIO_OBJ(req)
  DEF_AIO_OBJ(connect)
  DEF_AIO_OBJ(write)
  DEF_AIO_OBJ(shutdown)
  DEF_AIO_OBJ(udp_send)
  DEF_AIO_OBJ(fs)
  DEF_AIO_OBJ(work)
  DEF_AIO_OBJ(getaddrinfo)
  DEF_AIO_OBJ(cpu_info)
  DEF_AIO_OBJ(interface_address)
  DEF_AIO_OBJ(mutex)
  DEF_AIO_OBJ(rwlock)
  DEF_AIO_OBJ(sem)
  DEF_AIO_OBJ(cond)
  DEF_AIO_OBJ(barrier)
  {}
#undef DEF_AIO_OBJ
}
/**\memberof aio
   initialize all aio types with an event loop argument
   \param loop   PNAioLoop to uv_loop_t*, defaults to uv_default_loop()
   \see http://nikhilm.github.io/uvbook/networking.html#tcp
   \returns self */
static PN
aio_init_loop(Potion *P, PN cl, PN self, PN loop) {
  PN type = potion_send(PN_VTABLE(self), PN_name);
#define DEF_AIO_INIT(T) \
  if (type == PN_STR("Aio_"#T)) \
    return aio_##T##_init(P, cl, self, loop); \
  else
  DEF_AIO_INIT(tcp)
  DEF_AIO_INIT(udp)
  DEF_AIO_INIT(prepare)
  DEF_AIO_INIT(check)
  DEF_AIO_INIT(idle)
  DEF_AIO_INIT(timer)
  DEF_AIO_INIT(fs_poll)
  DEF_AIO_INIT(signal)
    potion_fatal("Invalid aio_init");
}

void Potion_Init_aio(Potion *P) {
  aio_vt = potion_type_new2(P, PN_TUSER, P->lobby, PN_STR("Aio"));
  potion_type_constructor_is(aio_vt, PN_FUNC(aio_new, "type=S"));
  //potion_class_method(aio_vt, "new", potion_aio_new, "type=S");
  potion_method(P->lobby, "aio", aio_new, "type=S");
  potion_method(aio_vt, "init", aio_init_loop, "|loop=o");
#define DEF_AIO_VT(T)                                                            \
  aio_##T##_vt = potion_type_new2(P, PN_TUSER, aio_vt, PN_STR("Aio_" _XSTR(t))); \
  potion_type_constructor_is(aio_##T##_vt, PN_FUNC(aio_##T##_new, 0));           \
  potion_method(aio_vt, "new", aio_##T##_new, 0);
#define DEF_AIO_INIT_VT(T,args) \
  DEF_AIO_VT(T); potion_method(aio_##T##_vt, "init", aio_##T##_init, args);

  DEF_AIO_INIT_VT(tcp, "|loop=o");
  DEF_AIO_INIT_VT(udp, "|loop=o");
  DEF_AIO_INIT_VT(prepare, "|loop=o");
  DEF_AIO_INIT_VT(check, "|loop=o");
  DEF_AIO_INIT_VT(idle,  "|loop=o");
  DEF_AIO_INIT_VT(timer, "|loop=o");
  DEF_AIO_INIT_VT(fs_poll, "|loop=o");
  DEF_AIO_INIT_VT(signal, "|loop=o");
  DEF_AIO_VT(loop);
  DEF_AIO_INIT_VT(tty,  "loop=o,file=o,readable=N");
  DEF_AIO_INIT_VT(pipe, "loop=o,ipc=N");
  DEF_AIO_INIT_VT(poll, "loop=o,fd=N");
  DEF_AIO_INIT_VT(async, "loop=o,cb=o");
  DEF_AIO_INIT_VT(fs_event, "loop=o,filename=S,cb=o,flags=N");
  DEF_AIO_INIT_VT(mutex, 0);
  DEF_AIO_INIT_VT(rwlock, 0);
  DEF_AIO_INIT_VT(cond, 0);
  DEF_AIO_INIT_VT(sem, "value=N");
  DEF_AIO_INIT_VT(barrier, "count=N");
  DEF_AIO_VT(handle);
  DEF_AIO_VT(process);
  DEF_AIO_VT(stream);
  DEF_AIO_VT(req);
  DEF_AIO_VT(connect);
  DEF_AIO_VT(write);
  DEF_AIO_VT(shutdown);
  DEF_AIO_VT(udp_send);
  DEF_AIO_VT(fs);
  DEF_AIO_VT(work);
  DEF_AIO_VT(getaddrinfo);
  DEF_AIO_VT(cpu_info);
  DEF_AIO_VT(interface_address);

#undef DEF_AIO_VT
#undef DEF_AIO_INIT_VT

  potion_method(aio_tcp_vt, "open", aio_tcp_open, "sock=N");
  potion_method(aio_tcp_vt, "nodelay", aio_tcp_nodelay, "enable=N");
  potion_method(aio_tcp_vt, "keepalive", aio_tcp_keepalive, "enable=N|delay:=0");
  potion_method(aio_tcp_vt, "simultaneous_accepts", aio_tcp_simultaneous_accepts, "enable=N");
  potion_method(aio_tcp_vt, "bind", aio_tcp_bind, "addr=S,port=N");
  potion_method(aio_tcp_vt, "bind6", aio_tcp_bind6, "addr=S,port=N");
  potion_method(aio_tcp_vt, "getsockname", aio_tcp_getsockname, 0);
  potion_method(aio_tcp_vt, "getpeername", aio_tcp_getpeername, 0);
  potion_method(aio_tcp_vt, "connect", aio_tcp_connect, "req=o,addr=S,port=N|connect_cb=&");
  potion_method(aio_tcp_vt, "connect6", aio_tcp_connect6, "req=o,addr=S,port=N|connect_cb=&");
  potion_method(aio_udp_vt, "open", aio_udp_open, "sockfd=N");
  potion_method(aio_udp_vt, "bind", aio_udp_bind, "addr=S,port=N");
  potion_method(aio_udp_vt, "bind6", aio_udp_bind6, "addr=S,port=N|ipv6only:=0");
  potion_method(aio_udp_vt, "getsockname", aio_udp_getsockname, 0);
  potion_method(aio_udp_vt, "set_membership", aio_udp_set_membership, "mcaddr=S,ifaddr=S,memb=o");
  potion_method(aio_udp_vt, "set_multicast_loop", aio_udp_set_multicast_loop, "on=N");
  potion_method(aio_udp_vt, "set_multicast_ttl", aio_udp_set_multicast_ttl, "ttl=N");
  potion_method(aio_udp_vt, "set_broadcast", aio_udp_set_broadcast, "on=N");
  potion_method(aio_udp_vt, "set_ttl", aio_udp_set_ttl, "ttl=N");
  potion_method(aio_udp_vt, "send", aio_udp_send, "req=o,buf=O,bufcnt=N,addr=S,port=N|send_cb=&");
  potion_method(aio_udp_vt, "send6", aio_udp_send6, "req=o,buf=O,bufcnt=N,addr=S,port=N|send_cb=&");
  potion_method(aio_udp_vt, "recv_start", aio_udp_recv_start, "recv_cb=&");
  potion_method(aio_udp_vt, "recv_stop", aio_udp_recv_stop, 0);
  potion_method(aio_pipe_vt, "open", aio_pipe_open, "fd=N");
  potion_method(aio_pipe_vt, "bind", aio_pipe_bind, "name=S");
  potion_method(aio_pipe_vt, "connect", aio_pipe_connect, "req=o,name=S|connect_cb=&");
  potion_method(aio_stream_vt, "write", aio_write, "req=o,buf=O,bufcnt=N|write_cb=&");
#if 0
  potion_method(aio_pipe_vt, "pending_instances", aio_pipe_pending_instances, "count=N");
#endif
}
