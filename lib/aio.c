/** \file lib/aio.c
  asynch event-driven non-blocking IO, via libuv, on files, network streams, processes.
  \class aio

  \see 3rd/libuv/include/uv.h and http://nikhilm.github.io/uvbook/basics.html

  The constructor calls the uv_T_init functions already.
  loop is always the last element, and defaults to uv_default_loop()

  For POSIX unbuffered fileio \see file.c open,read,write,seek calls on fd
  and lib/buffile.c for buffered IO.

 (c) 2013 perl11.org */
#include <stdio.h>
#include <stdlib.h>
#include <uv.h>
#include "p2.h"

PN aio_vt, aio_tcp_vt, aio_udp_vt, aio_loop_vt, aio_tty_vt, aio_pipe_vt, /*aio_poll_vt,*/ aio_prepare_vt,
  aio_idle_vt, aio_async_vt, aio_timer_vt, aio_fs_poll_vt, aio_signal_vt, aio_fs_event_vt, aio_mutex_vt,
  aio_rwlock_vt, aio_sem_vt, aio_cond_vt, aio_barrier_vt, aio_check_vt,
  aio_handle_vt, aio_process_vt, aio_stream_vt, aio_req_vt, aio_connect_vt, aio_write_vt, aio_shutdown_vt,
  aio_udp_send_vt, aio_fs_vt, aio_work_vt, aio_getaddrinfo_vt, aio_cpu_info_vt, aio_interface_address_vt;

#define AIO_DATA(T,ARG) \
  (aio_##T##_t*)PN_DATA(potion_fwd(ARG)); \
  if (PN_VTYPE(ARG) != aio_##T##_vt) return potion_type_error_want(P, ARG, ""_XSTR(T))
#define AIO_STREAM(ARG) \
  (aio_stream_t*)PN_DATA(potion_fwd(ARG)); \
  CHECK_AIO_STREAM(ARG)

//with wrapped callback
#define DEF_AIO_CB_WRAP(T) \
 DEF_AIO_CB_WRAP1(T,T,T)
#define DEF_AIO_CB_WRAP1(T,H,C)		\
typedef struct aio_##T##_s aio_##T##_t; \
struct aio_##T##_s {     \
  struct uv_##H##_s r;   \
  Potion *P;             \
  PN cl;                 \
  struct PNClosure *cb;	 \
}
//without
#define DEF_AIO_HANDLE_WRAP(T) \
typedef struct aio_##T##_s aio_##T##_t; \
struct aio_##T##_s {     \
  uv_##T##_t h;          \
  Potion *P;             \
  PN cl;                 \
}
typedef uv_buf_t aio_buf_t;
DEF_AIO_CB_WRAP(write);
DEF_AIO_CB_WRAP(connect);
DEF_AIO_CB_WRAP(shutdown);
DEF_AIO_CB_WRAP1(connection,stream,connection);
DEF_AIO_CB_WRAP1(close,handle,close);
//DEF_AIO_CB_WRAP(poll);
DEF_AIO_CB_WRAP(timer);
DEF_AIO_CB_WRAP(async);
DEF_AIO_CB_WRAP(prepare);
DEF_AIO_CB_WRAP(check);
DEF_AIO_CB_WRAP(idle);
DEF_AIO_CB_WRAP1(exit,process,exit);
DEF_AIO_CB_WRAP1(walk,handle,walk);
DEF_AIO_CB_WRAP(fs);
DEF_AIO_CB_WRAP(work);
DEF_AIO_CB_WRAP1(after_work,work,after_work);
DEF_AIO_CB_WRAP(getaddrinfo);
DEF_AIO_CB_WRAP(fs_poll);
DEF_AIO_CB_WRAP(signal);
DEF_AIO_HANDLE_WRAP(handle);
DEF_AIO_HANDLE_WRAP(loop);
DEF_AIO_CB_WRAP(fs_event);
DEF_AIO_CB_WRAP(pipe);
DEF_AIO_HANDLE_WRAP(process);
DEF_AIO_CB_WRAP1(stream,stream,read);
DEF_AIO_HANDLE_WRAP(tty);
DEF_AIO_HANDLE_WRAP(req);
DEF_AIO_CB_WRAP1(tcp,tcp,read);
DEF_AIO_CB_WRAP1(udp,udp,udp_recv);
DEF_AIO_CB_WRAP(udp_send);
DEF_AIO_HANDLE_WRAP(cpu_info);
DEF_AIO_HANDLE_WRAP(interface_address);
DEF_AIO_HANDLE_WRAP(sem);
DEF_AIO_HANDLE_WRAP(barrier);
DEF_AIO_HANDLE_WRAP(cond);
DEF_AIO_HANDLE_WRAP(mutex);
DEF_AIO_HANDLE_WRAP(rwlock);
#undef DEF_AIO_CB_WRAP
#undef DEF_AIO_HANDLE_WRAP

/**\memberof aio_fs_event
  The aio_fs_event callback will receive the following arguments:
  \param filename
  \param events UV_RENAME=1 or UV_CHANGE=2
  \param status
  \see http://nikhilm.github.io/uvbook/filesystem.html#file-change-events */
static void
aio_fs_event_cb(uv_fs_event_t* handle, const char* filename, int events, int status) {
  struct aio_fs_event_s* wrap = (struct aio_fs_event_s*)handle;
  vPN(Closure) cb = PN_CLOSURE(wrap->cb);
  char *data = (char*)(&wrap - sizeof(struct PNData) + sizeof(char*));
  if (cb) cb->method(wrap->P, wrap->cl, (PN)data, potion_str(wrap->P, filename),
		     PN_NUM(events), PN_NUM(status));
}

static PN aio_last_error(Potion *P, char *name, uv_loop_t* loop) {
  uv_err_t err = uv_last_error(loop);
  return potion_error(P, potion_str_format(P, "Error %s: %s", name,
                                           uv_strerror(err)), 0, 0, 0);
}

#define DEF_AIO_NEW_NOINIT(T)			\
  struct PNData *data = potion_data_alloc(P,	\
    sizeof(uv_##T##_t)+sizeof(Potion*)+sizeof(PN)+sizeof(void*)); \
  data->vt = aio_##T##_vt;			\
  ((aio_##T##_t*)PN_DATA(data))->P = P;		\
  ((aio_##T##_t*)PN_DATA(data))->cl = cl
#define DEF_AIO_NEW(T)				\
  uv_##T##_t *handle;				\
  DEF_AIO_NEW_NOINIT(T);			\
  handle = (uv_##T##_t*)PN_DATA(data)
#define DEF_AIO_NEW_LOOP(T)			\
  uv_loop_t* l;                                 \
  DEF_AIO_NEW(T);				\
  if (!loop) l = uv_default_loop();             \
  else if (PN_VTYPE(loop) == aio_loop_vt)	\
    l = (uv_loop_t*)PN_DATA(loop);		\
  else return potion_type_error(P, loop);
#define DEF_AIO_NEW_LOOP_INIT(T)		 \
  DEF_AIO_NEW_LOOP(T);                           \
  if (uv_##T##_init(l, handle))			 \
    return aio_last_error(P, "aio_"_XSTR(T), l); \
  return (PN)data;
#define AIO_CB_SET(T,ARG)		\
  uv_##T##_cb T##_cb;		\
  if (PN_IS_CLOSURE(cb)) {	\
    (ARG)->cb = PN_CLOSURE(cb);	\
    T##_cb = aio_##T##_cb;	\
  }				\
  else if (PN_IS_FFIPTR(cb))    \
    T##_cb = (uv_##T##_cb)cb;   \
  else T##_cb = aio_##T##_cb

#define CHECK_AIO_TYPE(self, type) \
  if (PN_VTYPE(self) != aio_##type##_vt) return potion_type_error_want(P, self, ""_XSTR(type))
#define CHECK_AIO_STREAM(stream) \
  { \
   PNType _t = PN_VTYPE(stream);  \
   if (_t != aio_stream_vt &&                               \
       _t != aio_tcp_vt &&                                  \
       _t != aio_udp_vt &&                                  \
       _t != aio_pipe_vt &&                                 \
       _t != aio_tty_vt)                                    \
     return potion_type_error_want(P, stream, "aio_stream"); \
  }

//cb wrappers
#define DEF_AIO_CB(T)			       \
  aio_##T##_t* wrap = (aio_##T##_t*)req;	       \
  vPN(Closure) cb = PN_CLOSURE(wrap->cb);              \
  PN data = (PN)((char*)&wrap - sizeof(struct PNData) + sizeof(char*)); \
  Potion *P = wrap->P;    \
  CHECK_AIO_TYPE(data,T); \
  if (cb) cb->method(P, wrap->cl, data, PN_NUM(status))

/**\class aio_tcp \memberof aio_tcp
   create and init a \c aio_tcp object
   \param loop   PNAioLoop to uv_loop_t*, defaults to uv_default_loop()
   \see http://nikhilm.github.io/uvbook/networking.html#tcp */
static PN aio_tcp_new(Potion *P, PN cl, PN self, PN loop) {
  //DEF_AIO_NEW_LOOP(tcp);
  uv_tcp_t *handle;
  struct PNData *data = potion_data_alloc(P,
    sizeof(uv_tcp_t)+sizeof(Potion*)+sizeof(PN)+sizeof(void*));
  uv_loop_t* l;
  data->vt = aio_tcp_vt;
  handle = (uv_tcp_t*)PN_DATA(data);
  if (!loop) l = uv_default_loop();
  else if (PN_VTYPE(loop) == aio_loop_vt) l = (uv_loop_t*)PN_DATA(loop);
  else return potion_type_error(P, loop);
  if (uv_tcp_init(l, handle))
    return aio_last_error(P, "aio_tcp", l);
  ((aio_tcp_t*)handle)->P = P;
  ((aio_tcp_t*)handle)->cl = cl;
  return (PN)data;
}
/**\class aio_udp \memberof aio_udp
   create and init a \c aio_udp object
   \param loop   PNAioLoop to uv_loop_t*, defaults to uv_default_loop()
   \see http://nikhilm.github.io/uvbook/networking.html#udp */
static PN aio_udp_new(Potion *P, PN cl, PN self, PN loop) {
  DEF_AIO_NEW_LOOP_INIT(udp);
}
static PN aio_prepare_new(Potion *P, PN cl, PN self, PN loop) {
  DEF_AIO_NEW_LOOP_INIT(prepare);
}
static PN aio_check_new(Potion *P, PN cl, PN self, PN loop) {
  DEF_AIO_NEW_LOOP_INIT(check);
}
static PN aio_idle_new(Potion *P, PN cl, PN self, PN loop) {
  DEF_AIO_NEW_LOOP_INIT(idle);
}
/**\class aio_timer \memberof aio_timer
   create and init a \c aio_timer object
   \param loop   PNAioLoop to uv_loop_t*, defaults to uv_default_loop()
   \see http://nikhilm.github.io/uvbook/utilities.html#timers */
static PN aio_timer_new(Potion *P, PN cl, PN self, PN loop) {
  DEF_AIO_NEW_LOOP_INIT(timer);
}
static PN aio_fs_poll_new(Potion *P, PN cl, PN self, PN loop) {
  DEF_AIO_NEW_LOOP_INIT(fs_poll);
}
static PN aio_signal_new(Potion *P, PN cl, PN self, PN loop) {
  DEF_AIO_NEW_LOOP_INIT(signal);
}
static PN aio_loop_new(Potion *P, PN cl, PN self) {
  uv_loop_t *l;
  uv_loop_t *def;
  struct PNData *data = potion_data_alloc(P,
    sizeof(uv_loop_t)+sizeof(Potion*)+sizeof(PN)+sizeof(void*));
  data->vt = aio_loop_vt;
  ((struct aio_loop_s*)data)->P = P;
  ((struct aio_loop_s*)data)->cl = cl;
  l = (uv_loop_t*)PN_DATA(data);
  def = uv_default_loop();
  memcpy(l, def, sizeof(uv_loop_t));
  return (PN)data;
}
static PN aio_handle_new(Potion *P, PN cl, PN self) {
  DEF_AIO_NEW_NOINIT(handle);
}
static PN aio_process_new(Potion *P, PN cl, PN self) {
  DEF_AIO_NEW_NOINIT(process);
}
static PN aio_stream_new(Potion *P, PN cl, PN self) {
  DEF_AIO_NEW_NOINIT(stream);
}
static PN aio_req_new(Potion *P, PN cl, PN self) {
  DEF_AIO_NEW_NOINIT(req);
}
static PN aio_connect_new(Potion *P, PN cl, PN self) {
  DEF_AIO_NEW_NOINIT(connect);
}
static PN aio_write_new(Potion *P, PN cl, PN self) {
  DEF_AIO_NEW_NOINIT(write);
}
static PN aio_shutdown_new(Potion *P, PN cl, PN self) {
  DEF_AIO_NEW_NOINIT(shutdown);
}
static PN aio_udp_send_new(Potion *P, PN cl, PN self) {
  DEF_AIO_NEW_NOINIT(udp_send);
}
static PN aio_fs_new(Potion *P, PN cl, PN self) {
  DEF_AIO_NEW_NOINIT(fs);
}
static PN aio_work_new(Potion *P, PN cl, PN self) {
  DEF_AIO_NEW_NOINIT(work);
}
static PN aio_getaddrinfo_new(Potion *P, PN cl, PN self) {
  DEF_AIO_NEW_NOINIT(getaddrinfo);
}
static PN aio_cpu_info_new(Potion *P, PN cl, PN self) {
  DEF_AIO_NEW_NOINIT(cpu_info);
}
static PN aio_interface_address_new(Potion *P, PN cl, PN self) {
  DEF_AIO_NEW_NOINIT(interface_address);
}
static PN aio_tty_new(Potion *P, PN cl, PN self, PN file, PN readable, PN loop) {
  DEF_AIO_NEW_LOOP(tty);
  if (uv_tty_init(l, handle, PN_NUM(PN_DATA(file)), PN_NUM(readable)))
    return aio_last_error(P, "aio_tty", l);
  return (PN)data;
}
static PN aio_pipe_new(Potion *P, PN cl, PN self, PN ipc, PN loop) {
  DEF_AIO_NEW_LOOP(pipe);
  if (uv_pipe_init(l, handle, PN_NUM(ipc)))
    return aio_last_error(P, "aio_pipe", l);
  return (PN)data;
}
/*static PN aio_poll_new(Potion *P, PN cl, PN self, PN fd, PN loop) {
  DEF_AIO_NEW_LOOP(poll);
  if (uv_poll_init((uv_loop_t*)loop, handle, PN_NUM(fd)))
    return aio_last_error(P, "aio_poll", l);
  return (PN)data;
  }*/
static PN aio_barrier_new(Potion *P, PN cl, PN self, PN count) {
  DEF_AIO_NEW(barrier);
  if (uv_barrier_init(handle, PN_NUM(count)))
    return potion_io_error(P, "aio_barrier");
  return (PN)data;
}
static PN aio_sem_new(Potion *P, PN cl, PN self, PN value) {
  DEF_AIO_NEW(sem);
  if (uv_sem_init(handle, PN_NUM(value)))
    return potion_io_error(P, "aio_sem");
  return (PN)data;
}
static PN aio_fs_event_new(Potion *P, PN cl, PN self, PN filename, PN cb, PN flags, PN loop) {
  DEF_AIO_NEW_LOOP(fs_event);
  uv_fs_event_cb fs_event_cb = aio_fs_event_cb;
  if (PN_IS_CLOSURE(cb)) handle->cb = (uv_fs_event_cb)cb;
  else if (PN_IS_FFIPTR(cb)) fs_event_cb = NULL;
  if (uv_fs_event_init(l, handle, PN_STR_PTR(filename), fs_event_cb, PN_NUM(flags)))
    return aio_last_error(P, "aio_fs_event", l);
  return (PN)data;
}
static void
aio_async_cb(uv_async_t* req, int status) {
  DEF_AIO_CB(async);
}
static PN aio_async_new(Potion *P, PN cl, PN self, PN cb, PN loop) {
  DEF_AIO_NEW_LOOP(async);
  AIO_CB_SET(async,(aio_tcp_t*)data);
  if (uv_async_init(l, handle, async_cb))
    return aio_last_error(P, "aio_async", l);
  return (PN)data;
}
static PN aio_cond_new(Potion *P, PN cl, PN self) {
  DEF_AIO_NEW(cond);
  if (uv_cond_init(handle))
    return potion_io_error(P, "aio_cond");
  return (PN)data;
}
static PN aio_mutex_new(Potion *P, PN cl, PN self) {
  DEF_AIO_NEW(mutex);
  if (uv_mutex_init(handle))
    return potion_io_error(P, "aio_mutex");
  return (PN)data;
}
static PN aio_rwlock_new(Potion *P, PN cl, PN self) {
  DEF_AIO_NEW(rwlock);
  if (uv_rwlock_init(handle))
    return potion_io_error(P, "aio_rwlock");
  return (PN)data;
}

#undef DEF_AIO_NEW_NOINIT
#undef DEF_AIO_NEW
#undef DEF_AIO_NEW_LOOP

static void
aio_write_cb(uv_write_t* req, int status) {
  DEF_AIO_CB(write);
}
static void
aio_connect_cb(uv_connect_t* req, int status) {
  DEF_AIO_CB(connect);
}
static void
aio_shutdown_cb(uv_shutdown_t* req, int status) {
  DEF_AIO_CB(shutdown);
}
static void
aio_prepare_cb(uv_prepare_t* req, int status) {
  DEF_AIO_CB(prepare);
}
static void
aio_check_cb(uv_check_t* req, int status) {
  DEF_AIO_CB(check);
}
static void
aio_connection_cb(uv_stream_t* req, int status) {
  DEF_AIO_CB(stream);
}
static void
aio_udp_send_cb(uv_udp_send_t* req, int status) {
  DEF_AIO_CB(udp_send);
}
static void
aio_idle_cb(uv_idle_t* req, int status) {
  DEF_AIO_CB(idle);
}
static void
aio_timer_cb(uv_timer_t* req, int status) {
  DEF_AIO_CB(timer);
}
#if 0 //yet unused
static void
aio_signal_cb(uv_signal_t* req, int status) { //signum really
  DEF_AIO_CB(signal);
}
static void
aio_fs_poll_cb(uv_fs_poll_t* handle, int status, const uv_stat_t* prev, const uv_stat_t* curr) {
  aio_fs_poll_t* wrap = (aio_fs_poll_t*)handle;
  vPN(Closure) cb = PN_CLOSURE(wrap->cb);
  PN data = (PN)((char*)&wrap - sizeof(struct PNData) + sizeof(char*));
  Potion *P = wrap->P;
  CHECK_AIO_TYPE(data,fs_poll);
  CHECK_AIO_TYPE(prev,stat);
  CHECK_AIO_TYPE(curr,stat);
  PN_CHECK_INT(status);
  if (cb) cb->method(P, wrap->cl, (PN)data, PN_NUM(status),
		     potion_ref(wrap->P, (PN)prev), potion_ref(wrap->P, (PN)curr));
}
#endif
static void
aio_read_cb(uv_stream_t* stream, ssize_t nread, uv_buf_t buf) {
  aio_stream_t* wrap = (aio_stream_t*)stream;
  vPN(Closure) cb = PN_CLOSURE(wrap->cb);
  PN data = (PN)((char*)&wrap - sizeof(struct PNData) + sizeof(char*));
  Potion *P = wrap->P;
  CHECK_AIO_STREAM(data);
  if (cb)
    cb->method(P, wrap->cl, (PN)data, PN_NUM(nread),
               potion_byte_str2(wrap->P, buf.base, buf.len));
}
static void
aio_read2_cb(uv_pipe_t* pipe, ssize_t nread, uv_buf_t buf, uv_handle_type pending) {
  aio_pipe_t* wrap = (aio_pipe_t*)pipe;
  vPN(Closure) cb = PN_CLOSURE(wrap->cb);
  PN data = (PN)((char*)&wrap - sizeof(struct PNData) + sizeof(char*));
  Potion *P = wrap->P;
  CHECK_AIO_TYPE(data, pipe);
  if (cb)
    cb->method(P, wrap->cl, (PN)data, PN_NUM(nread),
	       potion_byte_str2(wrap->P, buf.base, buf.len), PN_NUM(pending));
}

/** Returns the libuv version packed into a single integer. 8 bits are used for
 * each component, with the patch number stored in the 8 least significant
 * bits. E.g. for libuv 1.2.3 this would return 0x010203. */
static PN
aio_version(Potion *P, PN cl, PN self) {
  return PN_NUM(uv_version());
}
/** Returns the libuv version number as a string. For non-release versions
 * "-pre" is appended, so the version number could be "1.2.3-pre". */
static PN
aio_version_string(Potion *P, PN cl, PN self) {
  return PN_STR(uv_version_string());
}

static PN
aio_run(Potion *P, PN cl, PN self, PN loop, PN mode) {
  uv_loop_t* l;
  if (!loop) l = uv_default_loop();
  else if (PN_VTYPE(loop) == aio_loop_vt) l = (uv_loop_t*)PN_DATA(loop);
  else return potion_type_error(P, loop);
  if (mode) PN_CHECK_TYPE(mode,PN_TNUMBER);
  return uv_run(l, mode ? PN_INT(mode) : UV_RUN_DEFAULT)
    ? aio_last_error(P, "run", l) : self;
}

static PN
aio_tcp_open(Potion *P, PN cl, PN tcp, PN sock) {
  aio_tcp_t *handle = AIO_DATA(tcp,tcp);
  PN_CHECK_TYPE(sock,PN_TNUMBER);
  return uv_tcp_open(&handle->r, PN_INT(sock))
    ? aio_last_error(P, "tcp open", handle->r.loop) : tcp;
}
static PN
aio_tcp_nodelay(Potion *P, PN cl, PN tcp, PN enable) {
  aio_tcp_t *handle = AIO_DATA(tcp,tcp);
  PN_CHECK_TYPE(enable,PN_TNUMBER);
  return uv_tcp_nodelay(&handle->r, PN_INT(enable))
    ? aio_last_error(P, "tcp nodelay", handle->r.loop) : tcp;
}
static PN
aio_tcp_keepalive(Potion *P, PN cl, PN tcp, PN enable, PN delay) {
  aio_tcp_t *handle = AIO_DATA(tcp,tcp);
  PN_CHECK_TYPE(enable,PN_TNUMBER);
  PN_CHECK_TYPE(delay,PN_TNUMBER);
  return uv_tcp_keepalive(&handle->r, PN_INT(enable), PN_INT(delay))
    ? aio_last_error(P, "tcp keepalive", handle->r.loop) : tcp;
}
static PN
aio_tcp_simultaneous_accepts(Potion *P, PN cl, PN tcp, PN enable) {
  aio_tcp_t *handle = AIO_DATA(tcp,tcp);
  PN_CHECK_INT(enable); //TODO bool
  return uv_tcp_simultaneous_accepts(&handle->r, PN_INT(enable))
    ? aio_last_error(P, "tcp simultaneous_accepts", handle->r.loop) : tcp;
}
static PN
aio_tcp_bind(Potion *P, PN cl, PN tcp, PN addr, PN port) {
  aio_tcp_t *handle = AIO_DATA(tcp,tcp);
  PN_CHECK_STR(addr);
  PN_CHECK_INT(port);
  struct sockaddr_in ip4 = uv_ip4_addr(PN_STR_PTR(addr), PN_INT(port));
  return uv_tcp_bind(&handle->r, ip4)
    ? aio_last_error(P, "tcp bind", handle->r.loop) : tcp;
}
static PN
aio_tcp_bind6(Potion *P, PN cl, PN tcp, PN addr, PN port) {
  aio_tcp_t *handle = AIO_DATA(tcp,tcp);
  PN_CHECK_STR(addr);
  PN_CHECK_INT(port);
  struct sockaddr_in6 ip6 = uv_ip6_addr(PN_STR_PTR(addr), PN_INT(port));
  return uv_tcp_bind6(&handle->r, ip6)
    ? aio_last_error(P, "tcp bind6", handle->r.loop) : tcp;
}
static PN
aio_tcp_getsockname(Potion *P, PN cl, PN tcp) {
  struct sockaddr sock; int len;
  aio_tcp_t *handle = AIO_DATA(tcp,tcp);
  return (uv_tcp_getsockname(&handle->r, &sock, &len))
    ? aio_last_error(P, "tcp getsockname", handle->r.loop)
    : potion_str2(P, sock.sa_data, len);
}
static PN
aio_tcp_getpeername(Potion *P, PN cl, PN tcp) {
  struct sockaddr sock; int len;
  aio_tcp_t *handle = AIO_DATA(tcp,tcp);
  return (uv_tcp_getpeername(&handle->r, &sock, &len))
    ? aio_last_error(P, "tcp getpeername", handle->r.loop)
    : potion_str2(P, sock.sa_data, len);
}
static PN
aio_tcp_connect(Potion *P, PN cl, PN tcp, PN req, PN addr, PN port, PN cb) {
  aio_tcp_t *handle = AIO_DATA(tcp,tcp);
  aio_connect_t *request = AIO_DATA(connect,req);
  PN_CHECK_STR(addr);
  PN_CHECK_INT(port);
  struct sockaddr_in ip4 = uv_ip4_addr(PN_STR_PTR(addr), PN_INT(port));
  AIO_CB_SET(connect,request);
  return uv_tcp_connect(&request->r, &handle->r, ip4, connect_cb)
    ? aio_last_error(P, "tcp connect", handle->r.loop)
    : tcp;
}
static PN
aio_tcp_connect6(Potion *P, PN cl, PN tcp, PN req, PN addr, PN port, PN cb) {
  aio_tcp_t *handle = AIO_DATA(tcp,tcp);
  aio_connect_t *request = AIO_DATA(connect,req);
  PN_CHECK_STR(addr);
  PN_CHECK_INT(port);
  struct sockaddr_in6 ip6 = uv_ip6_addr(PN_STR_PTR(addr), PN_INT(port));
  AIO_CB_SET(connect,request);
  return uv_tcp_connect6(&request->r, &handle->r, ip6, connect_cb)
    ? aio_last_error(P, "tcp connect6", handle->r.loop)
    : tcp;
}
///\returns aio_udp or nil
static PN
aio_udp_open(Potion *P, PN cl, PN udp, PN sockfd) {
  aio_udp_t *handle = AIO_DATA(udp,udp);
  PN_CHECK_INT(sockfd);
  return uv_udp_open(&handle->r, PN_INT(sockfd))
    ? aio_last_error(P, "udp open", handle->r.loop)
    : udp;
}
static PN
aio_udp_bind(Potion *P, PN cl, PN udp, PN addr, PN port) {
  aio_udp_t *handle = AIO_DATA(udp,udp);
  PN_CHECK_STR(addr);
  PN_CHECK_INT(port);
  struct sockaddr_in ip4 = uv_ip4_addr(PN_STR_PTR(addr), PN_INT(port));
  return uv_udp_bind(&handle->r, ip4, 0)
    ? aio_last_error(P, "udp bind", handle->r.loop)
    : udp;
}
static PN
aio_udp_bind6(Potion *P, PN cl, PN udp, PN addr, PN port, PN flags) {
  aio_udp_t *handle = AIO_DATA(udp,udp);
  PN_CHECK_STR(addr);
  PN_CHECK_INT(port);
  PN_CHECK_INT(flags);
  struct sockaddr_in6 ip6 = uv_ip6_addr(PN_STR_PTR(addr), PN_INT(port));
  return uv_udp_bind6(&handle->r, ip6, PN_INT(flags))
    ? aio_last_error(P, "udp bind6", handle->r.loop)
    : udp;
}
static PN
aio_udp_getsockname(Potion *P, PN cl, PN udp) {
  struct sockaddr sock; int len;
  aio_udp_t *handle = AIO_DATA(udp,udp);
  return uv_udp_getsockname(&handle->r, &sock, &len)
    ? aio_last_error(P, "udp getsockname", handle->r.loop)
    : potion_str2(P, sock.sa_data, len);
}
static PN
aio_udp_set_membership(Potion *P, PN cl, PN udp, PN mcaddr, PN ifaddr, PN membership) {
  aio_udp_t *handle = AIO_DATA(udp,udp);
  PN_CHECK_STR(mcaddr);
  PN_CHECK_STR(ifaddr);
  PN_CHECK_INT(membership);
  return uv_udp_set_membership(&handle->r, PN_STR_PTR(mcaddr), PN_STR_PTR(ifaddr), PN_NUM(membership))
    ? aio_last_error(P, "udp set_membership", handle->r.loop)
    : udp;
}
static PN
aio_udp_set_multicast_loop(Potion *P, PN cl, PN udp, PN on) {
  aio_udp_t *handle = AIO_DATA(udp,udp);
  PN_CHECK_INT(on);  //TODO boolean
  return uv_udp_set_multicast_loop(&handle->r, PN_NUM(on))
    ? aio_last_error(P, "udp set_multicast_loop", handle->r.loop)
    : udp;
}
static PN
aio_udp_set_multicast_ttl(Potion *P, PN cl, PN udp, PN ttl) {
  aio_udp_t *handle = AIO_DATA(udp,udp);
  PN_CHECK_INT(ttl);
  return uv_udp_set_multicast_ttl(&handle->r, PN_NUM(ttl))
    ? aio_last_error(P, "udp set_multicast_ttl", handle->r.loop)
    : udp;
}
static PN
aio_udp_set_broadcast(Potion *P, PN cl, PN udp, PN on) {
  aio_udp_t *handle = AIO_DATA(udp,udp);
  PN_CHECK_INT(on);  //TODO boolean
  return uv_udp_set_broadcast(&handle->r, PN_NUM(on))
    ? aio_last_error(P, "udp set_broadcast", handle->r.loop)
    : udp;
}
static PN
aio_udp_set_ttl(Potion *P, PN cl, PN udp, PN ttl) {
  aio_udp_t *handle = AIO_DATA(udp,udp);
  PN_CHECK_INT(ttl);
  return uv_udp_set_ttl(&handle->r, PN_NUM(ttl))
    ? aio_last_error(P, "udp set_ttl", handle->r.loop)
    : udp;
}

static PN
aio_udp_send(Potion *P, PN cl, PN udp, PN req, PN buf, PN bufcnt, PN addr, PN port, PN cb) {
  aio_udp_t *handle = AIO_DATA(udp,udp);
  aio_udp_send_t *request = AIO_DATA(udp_send,req);
  PN_CHECK_STR(addr);
  PN_CHECK_INT(port);
  struct sockaddr_in ip4 = uv_ip4_addr(PN_STR_PTR(addr), PN_INT(port));
  //CHECK_AIO_TYPE(buf,buf); //FIXME buf as PNBytes
  PN_CHECK_INT(bufcnt);
  uv_buf_t *bufs = (uv_buf_t*)PN_DATA(potion_fwd(buf));
  AIO_CB_SET(udp_send,request);
  return uv_udp_send(&request->r, &handle->r, bufs, bufcnt, ip4, udp_send_cb)
    ? aio_last_error(P, "udp send", handle->r.loop)
    : udp;
}
static PN
aio_udp_send6(Potion *P, PN cl, PN udp, PN req, PN buf, PN bufcnt, PN addr, PN port, PN cb) {
  aio_udp_t *handle = AIO_DATA(udp,udp);
  aio_udp_send_t *request = AIO_DATA(udp_send, req);
  PN_CHECK_STR(addr);
  PN_CHECK_INT(port);
  struct sockaddr_in6 ip6 = uv_ip6_addr(PN_STR_PTR(addr), PN_INT(port));
  //CHECK_AIO_TYPE(buf,buf); //FIXME buf as PNBytes
  PN_CHECK_INT(bufcnt);
  uv_buf_t *bufs = (uv_buf_t*)PN_DATA(potion_fwd(buf));
  AIO_CB_SET(udp_send,request);
  return uv_udp_send6(&request->r, &handle->r, bufs, bufcnt, ip6, udp_send_cb)
    ? aio_last_error(P, "udp send6", handle->r.loop)
    : udp;
}

static uv_buf_t aio_alloc_cb(uv_handle_t* handle, size_t size) {
  aio_handle_t* wrap = (aio_handle_t*)handle;
  struct PNBytes *buf = potion_gc_alloc(wrap->P, PN_TBYTES, size);
  return (uv_buf_t)uv_buf_init(buf->chars, size);
}

/**\memberof aio_udp
   This callback is invoked when a new UDP datagram is received.
   \param handle aio_udp handle
   \param nread int Number of bytes that have been received.
     0 if there is no more data to read. You may
     discard or repurpose the read buffer.
     -1 if a transmission error was detected.
   \param buf PNBytes with the received data
   \param addr PNString
   \param port PNNumber
   \param flags PNNumber, One or more OR'ed AIO_UDP_* constants,
      so far only AIO_UDP_PARTIAL is used.
   \see http://nikhilm.github.io/uvbook/networking.html#udp */
static void
aio_udp_recv_cb(uv_udp_t* handle, ssize_t nread, uv_buf_t buf,
    struct sockaddr* addr, unsigned flags) {
  struct aio_udp_s* wrap = (struct aio_udp_s*)handle;
  char ip[46];
  int port;
  struct sockaddr_in* addr_in = (struct sockaddr_in*)addr;
  vPN(Closure) cb = PN_CLOSURE(wrap->r.recv_cb);
  char *data = (char*)(&wrap - sizeof(struct PNData) + sizeof(char*));
  if (addr_in->sin_family == AF_INET6) {
    uv_ip6_name((struct sockaddr_in6*)addr, ip, 46);
    port = ((struct sockaddr_in6*)addr)->sin6_port;
  } else {
    uv_ip4_name(addr_in, ip, 46);
    port = addr_in->sin_port;
  }
  //TODO: maybe reuse PNBytes with buf structure
  if (cb)
    cb->method(wrap->P, wrap->cl, (PN)data, PN_NUM(nread),
	       potion_byte_str2(wrap->P, buf.base, buf.len),
	       potion_byte_str(wrap->P, ip),
	       PN_NUM(port), PN_NUM(flags));
}

static PN
aio_udp_recv_start(Potion *P, PN cl, PN udp, PN cb) {
  aio_udp_t *handle = AIO_DATA(udp,udp);
  AIO_CB_SET(udp_recv,handle);
  return uv_udp_recv_start(&handle->r, aio_alloc_cb, udp_recv_cb)
    ? aio_last_error(P, "udp recv_start", handle->r.loop)
    : udp;
}
static PN
aio_udp_recv_stop(Potion *P, PN cl, PN udp) {
  aio_udp_t *handle = AIO_DATA(udp,udp);
  return uv_udp_recv_stop(&handle->r)
    ? aio_last_error(P, "udp recv_stop", handle->r.loop)
    : udp;
}
static PN
aio_prepare_start(Potion *P, PN cl, PN self, PN cb) {
  aio_prepare_t *handle = AIO_DATA(prepare,self);
  AIO_CB_SET(prepare,handle);
  return uv_prepare_start(&handle->r, prepare_cb)
    ? aio_last_error(P, "prepare start", handle->r.loop)
    : self;
}
static PN
aio_prepare_stop(Potion *P, PN cl, PN self) {
  aio_prepare_t *handle = AIO_DATA(prepare,self);
  return uv_prepare_stop(&handle->r)
    ? aio_last_error(P, "prepare stop", handle->r.loop)
    : self;
}
static PN
aio_check_start(Potion *P, PN cl, PN self, PN cb) {
  aio_check_t *handle = AIO_DATA(check, self);
  AIO_CB_SET(check,handle);
  return uv_check_start(&handle->r, check_cb)
    ? aio_last_error(P, "check start", handle->r.loop)
    : self;
}
static PN
aio_check_stop(Potion *P, PN cl, PN self) {
  aio_check_t *handle = AIO_DATA(check, self);
  return uv_check_stop(&handle->r)
    ? aio_last_error(P, "check stop", handle->r.loop)
    : self;
}
static PN
aio_idle_start(Potion *P, PN cl, PN self, PN cb) {
  aio_idle_t *handle = AIO_DATA(idle, self);
  AIO_CB_SET(idle,handle);
  return uv_idle_start(&handle->r, idle_cb)
    ? aio_last_error(P, "idle start", handle->r.loop)
    : self;
}
static PN
aio_idle_stop(Potion *P, PN cl, PN self) {
  aio_idle_t *handle = AIO_DATA(idle, self);
  return uv_idle_stop(&handle->r)
    ? aio_last_error(P, "idle stop", handle->r.loop)
    : self;
}

/// Set mode. 0 for normal, 1 for raw.
static PN
aio_tty_set_mode(Potion *P, PN cl, PN tty, PN mode) {
  aio_tty_t *handle = AIO_DATA(tty,tty);
  PN_CHECK_INT(mode);
  return uv_tty_set_mode(&handle->h, PN_INT(mode))
    ? aio_last_error(P, "tty set_mode", handle->h.loop)
    : tty;
}
/// To be called when the program exits. Resets TTY settings to default
/// values for the next process to take over.
static PN
aio_tty_reset_mode(Potion *P, PN cl, PN tty) {
  uv_tty_reset_mode();
  return tty;
}
static PN
aio_tty_get_winsize(Potion *P, PN cl, PN tty) {
  aio_tty_t *handle = AIO_DATA(tty,tty);
  int width, height;
  if (uv_tty_get_winsize(&handle->h, &width, &height))
    aio_last_error(P, "tty get_winsize", handle->h.loop);
  else
    return PN_PUSH(PN_PUSH(PN_TUP0(), PN_NUM(width)), PN_NUM(height));
}
static PN
aio_guess_handle(Potion *P, PN cl, PN file) {
  PN_CHECK_INT(file);
  uv_handle_type r = uv_guess_handle(PN_INT(file));
  return PN_NUM(r);
}

static PN
aio_pipe_open(Potion *P, PN cl, PN pipe, PN file) {
  aio_pipe_t *handle = AIO_DATA(pipe,pipe);
  return uv_pipe_open(&handle->r, PN_INT(file))
    ? aio_last_error(P, "pipe open", handle->r.loop)
    : pipe;
}
static PN
aio_pipe_bind(Potion *P, PN cl, PN pipe, PN name) {
  aio_pipe_t *handle = AIO_DATA(pipe,pipe);
  PN_CHECK_STR(name);
  return uv_pipe_bind(&handle->r, PN_STR_PTR(name))
    ? aio_last_error(P, "pipe bind", handle->r.loop)
    : pipe;
}
static PN
aio_pipe_connect(Potion *P, PN cl, PN pipe, PN req, PN name, PN cb) {
  aio_pipe_t *handle = AIO_DATA(pipe,pipe);
  aio_connect_t *request = AIO_DATA(connect,req);
  AIO_CB_SET(connect,request);
  PN_CHECK_STR(name);
  uv_pipe_connect(&request->r, &handle->r, PN_STR_PTR(name), connect_cb);
  return pipe;
}
// Windows only
static PN
aio_pipe_pending_instances(Potion *P, PN cl, PN pipe, PN count) {
  aio_pipe_t *handle = AIO_DATA(pipe,pipe);
  PN_CHECK_INT(count);
  uv_pipe_pending_instances(&handle->r, PN_INT(count));
  return pipe;
}

static PN
aio_shutdown(Potion *P, PN cl, PN stream, PN req, PN cb) {
  aio_stream_t *handle = AIO_DATA(stream,stream);
  aio_shutdown_t *request = AIO_DATA(shutdown,req);
  AIO_CB_SET(shutdown,request);
  return uv_shutdown(&request->r, &handle->r, shutdown_cb)
    ? aio_last_error(P, "shutdown", handle->r.loop)
    : stream;
}

/**
   TODO: buf should be PNBytes, not PNData wrapping uv_buf_t
 */
static PN
aio_write(Potion *P, PN cl, PN stream, PN req, PN buf, PN bufcnt, PN cb) {
  //CHECK_AIO_TYPE(buf,buf); //FIXME buf as PNBytes
  PN_CHECK_INT(bufcnt);
  aio_stream_t *stm = AIO_DATA(stream,stream);
  aio_write_t *request = AIO_DATA(write,req);
  uv_buf_t *bufs = (uv_buf_t*)PN_DATA(potion_fwd(buf));
  AIO_CB_SET(write,request);
  return uv_write(&request->r, &stm->r, bufs, PN_INT(bufcnt), write_cb)
    ? aio_last_error(P, "write", stm->r.loop)
    : stream;
}

static PN
aio_listen(Potion *P, PN cl, PN stream, PN backlog, PN cb) {
  aio_stream_t *request = AIO_STREAM(stream);
  AIO_CB_SET(connection,request);
  PN_CHECK_INT(backlog);
  return uv_listen(&request->r, PN_INT(backlog), connection_cb)
    ? aio_last_error(P, "listen", request->r.loop)
    : stream;
}
/**
 * This call is used in conjunction with listen() to accept incoming
 * connections. Call uv_accept after receiving a uv_connection_cb to accept
 * the connection.
 *
 * When the uv_connection_cb is called it is guaranteed that uv_accept will
 * complete successfully the first time. If you attempt to use it more than
 * once, it may fail. It is suggested to only call uv_accept once per
 * uv_connection_cb call. */
static PN
aio_accept(Potion *P, PN cl, PN stream, PN client) {
  aio_stream_t *stream_u = AIO_STREAM(stream);
  aio_stream_t *client_u = AIO_STREAM(client);
  return uv_accept(&stream_u->r, &client_u->r)
    ? aio_last_error(P, "accept", stream_u->r.loop)
    : stream;
}
/**
 * Read data from an incoming stream. The callback will be made several
 * times until there is no more data to read or read_stop is called.
 * When we've reached EOF nread will be set to -1 and the error is set
 * to UV_EOF. When nread == -1 the buf parameter might not point to a
 * valid buffer; in that case buf.len and buf.base are both set to 0.
 * Note that nread might also be 0, which does *not* indicate an error or
 * eof; it happens when aio requested a buffer through the alloc callback
 * but then decided that it didn't need that buffer.
 */
static PN
aio_read_start(Potion *P, PN cl, PN self, PN cb) {
  aio_stream_t *handle = AIO_STREAM(self);
  AIO_CB_SET(read,handle);
  return uv_read_start(&handle->r, aio_alloc_cb, read_cb)
    ? aio_last_error(P, "read start", handle->r.loop)
    : self;
}
static PN
aio_read_stop(Potion *P, PN cl, PN self) {
  aio_stream_t *handle = AIO_STREAM(self);
  return uv_read_stop(&handle->r)
    ? aio_last_error(P, "read stop", handle->r.loop)
    : self;
}
static PN
aio_read2_start(Potion *P, PN cl, PN self, PN cb) {
  aio_pipe_t *handle = AIO_DATA(pipe,self);
  AIO_CB_SET(read2,handle);
  return uv_read2_start((uv_stream_t*)&handle->r, aio_alloc_cb, read2_cb)
    ? aio_last_error(P, "read2 start", handle->r.loop)
    : self;
}
/** \memberof aio_stream
 * Extended write function for sending handles over a pipe. The pipe must be
 * initialized with ipc == 1.
 * send_handle must be a TCP socket or pipe, which is a server or a connection
 * (listening or connected state).  Bound sockets or pipes will be assumed to
 * be servers.
 */
static PN
aio_write2(Potion *P, PN cl, PN stream, PN req, PN buf, PN bufcnt, PN send_handle, PN cb) {
  //CHECK_AIO_TYPE(buf,buf); //FIXME buf as PNBytes
  PN_CHECK_INT(bufcnt);
  aio_stream_t *stm = AIO_STREAM(stream);
  aio_write_t *request = AIO_DATA(write,req);
  aio_stream_t *handle = AIO_STREAM(send_handle);
  uv_buf_t *bufs = (uv_buf_t*)PN_DATA(potion_fwd(buf));
  AIO_CB_SET(write,request);
  return uv_write2(&request->r, &stm->r, bufs, PN_INT(bufcnt), &handle->r, write_cb)
    ? aio_last_error(P, "write", stm->r.loop)
    : stream;
}
/** \memberof aio_stream
    \returns PNBoolean */
static PN
aio_is_readable(Potion *P, PN cl, PN stream) {
  const aio_stream_t *handle = AIO_STREAM(stream);
  return uv_is_readable(&handle->r) ? PN_TRUE : PN_FALSE;
}
/** \memberof aio_stream
    \returns PNBoolean */
static PN
aio_is_writable(Potion *P, PN cl, PN stream) {
  const aio_stream_t *handle = AIO_STREAM(stream);
  return uv_is_writable(&handle->r) ? PN_TRUE : PN_FALSE;
}
/** \memberof aio_stream
 * Used to determine whether a stream is closing or closed.
 *
 * N.B. is only valid between the initialization of the handle
 *      and the arrival of the close callback, and cannot be used
 *      to validate the handle.
 * \returns PNBoolean */
static PN
aio_is_closing(Potion *P, PN cl, PN stream) {
  const aio_stream_t *handle = AIO_STREAM(stream);
  return uv_is_closing((const uv_handle_t*)&handle->r) ? PN_TRUE : PN_FALSE;
}

/*
 * This can be called from other threads to wake up a libuv thread.
 * aio/libuv is single threaded at the moment. */
static PN
aio_async_send(Potion *P, PN cl, PN self) {
  aio_async_t *handle = AIO_DATA(async,self);
  return PN_NUM(uv_async_send(&handle->r));
}
static PN
aio_timer_start(Potion *P, PN cl, PN self, PN cb, PN timeout, PN repeat) {
  aio_timer_t *handle = AIO_DATA(timer,self);
  PN_CHECK_INT(timeout);
  PN_CHECK_INT(repeat);
  AIO_CB_SET(timer,handle);
  return uv_timer_start(&handle->r, timer_cb, PN_INT(timeout), PN_INT(repeat))
    ? aio_last_error(P, "timer start", handle->r.loop)
    : self;
}
static PN
aio_timer_stop(Potion *P, PN cl, PN self) {
  aio_timer_t *handle = AIO_DATA(timer,self);
  return uv_timer_stop(&handle->r)
    ? aio_last_error(P, "timer stop", handle->r.loop)
    : self;
}
/** \memberof aio_timer
 * Stop the timer, and if it is repeating restart it using the repeat value
 * as the timeout. If the timer has never been started before it returns -1 and
 * sets the error to UV_EINVAL.
 */
static PN
aio_timer_again(Potion *P, PN cl, PN self) {
  aio_timer_t *handle = AIO_DATA(timer,self);
  return uv_timer_again(&handle->r)
    ? aio_last_error(P, "timer again", handle->r.loop)
    : self;
}
static PN
aio_timer_get_repeat(Potion *P, PN cl, PN self) {
  aio_timer_t *handle = AIO_DATA(timer,self);
  return PN_NUM(uv_timer_get_repeat(&handle->r));
}
/*
 * Set the repeat value in milliseconds. Note that if the repeat value is set
 * from a timer callback it does not immediately take effect. If the timer was
 * non-repeating before, it will have been stopped. If it was repeating, then
 * the old repeat value will have been used to schedule the next timeout.
 */
static PN
aio_timer_set_repeat(Potion *P, PN cl, PN self, PN repeat) {
  aio_timer_t *handle = AIO_DATA(timer,self);
  PN_CHECK_INT(repeat);
  uv_timer_set_repeat(&handle->r, PN_INT(repeat));
  return self;
}
#undef AIO_CB_SET

void Potion_Init_aio(Potion *P) {
  aio_vt = potion_type_new(P, PN_TUSER, P->lobby); \
  potion_define_global(P, PN_STR("Aio"), aio_vt);
#define DEF_AIO_VT(T,paren) \
  aio_##T##_vt = potion_type_new(P, PN_TUSER, paren##_vt); \
  potion_define_global(P, PN_STR("Aio_" _XSTR(T)), aio_##T##_vt); \
  potion_method(aio_##T##_vt, ""_XSTR(T), aio_##T##_new, 0)
#define DEF_AIO_GLOBAL_VT(T,paren,args) \
  DEF_AIO_VT(T,paren); \
  potion_type_call_is(aio_##T##_vt, PN_FUNC(aio_##T##_new, args)); \
  potion_type_constructor_is(aio_##T##_vt, PN_FUNC(aio_##T##_new, args)); \
  potion_method(P->lobby, "aio_"_XSTR(T), aio_##T##_new, args)

  potion_define_global(P, PN_STR("AIO_RUN_DEFAULT"), PN_NUM(0));
  potion_define_global(P, PN_STR("AIO_RUN_ONCE"),   PN_NUM(1));
  potion_define_global(P, PN_STR("AIO_RUN_NOWAIT"), PN_NUM(2));
  potion_define_global(P, PN_STR("AIO_UDP_IPV6ONLY"), PN_NUM(1));
  potion_define_global(P, PN_STR("AIO_UDP_PARTIAL"), PN_NUM(2));
  potion_method(P->lobby, "aio_version", aio_version, 0);
  potion_method(P->lobby, "aio_version_string", aio_version_string, 0);
  potion_method(aio_vt, "version", aio_version, 0);
  potion_method(aio_vt, "version_string", aio_version_string, 0);

  DEF_AIO_VT(handle,aio);
  DEF_AIO_VT(stream,aio);
  DEF_AIO_GLOBAL_VT(tcp,aio_stream,"|loop=o");
  DEF_AIO_GLOBAL_VT(udp,aio_stream,"|loop=o");
  DEF_AIO_GLOBAL_VT(tty,aio_stream,"file=o,readable=N|loop=o");
  DEF_AIO_GLOBAL_VT(pipe,aio_stream,"ipc=N|loop=o");
  DEF_AIO_GLOBAL_VT(prepare,aio,"|loop=o");
  DEF_AIO_GLOBAL_VT(check,aio,"|loop=o");
  DEF_AIO_GLOBAL_VT(idle,aio,"|loop=o");
  DEF_AIO_GLOBAL_VT(timer,aio,"|loop=o");
  DEF_AIO_GLOBAL_VT(fs_poll,aio,"|loop=o");
  DEF_AIO_GLOBAL_VT(signal,aio,"|loop=o");
  //DEF_AIO_GLOBAL_VT(poll,aio,"fd=N|loop=o");
  DEF_AIO_GLOBAL_VT(async,aio,"cb=o|loop=o");
  DEF_AIO_GLOBAL_VT(fs_event,aio,"filename=S,cb=o,flags=N|loop=o");
  DEF_AIO_GLOBAL_VT(mutex,aio,0);
  DEF_AIO_GLOBAL_VT(rwlock,aio,0);
  DEF_AIO_GLOBAL_VT(cond,aio,0);
  DEF_AIO_GLOBAL_VT(sem,aio,"value=N");
  DEF_AIO_GLOBAL_VT(barrier,aio,"count=N");
  DEF_AIO_GLOBAL_VT(loop,aio,0);
  DEF_AIO_GLOBAL_VT(process,aio,0);
  DEF_AIO_VT(req,aio);
  DEF_AIO_VT(connect,aio_stream);
  DEF_AIO_VT(write,aio);
  DEF_AIO_VT(shutdown,aio);
  DEF_AIO_VT(udp_send,aio_udp);
  DEF_AIO_VT(fs,aio);
  DEF_AIO_VT(work,aio);
  DEF_AIO_VT(getaddrinfo,aio);
  DEF_AIO_VT(cpu_info,aio);
  DEF_AIO_VT(interface_address,aio);

#undef DEF_AIO_VT
#undef DEF_AIO_INIT_VT

  potion_method(aio_vt, "run", aio_run, "|loop=o,mode=N");
  potion_method(aio_stream_vt, "write", aio_write, "req=o,buf=O,bufcnt=N|write_cb=&");
  potion_method(aio_stream_vt, "shutdown", aio_shutdown, "req=o|shutdown_cb=&");
  //potion_method(aio_shutdown_vt, "shutdown", aio_shutdown, "stream=o|cb=&");
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
  potion_method(aio_tty_vt, "set_mode", aio_tty_set_mode, "mode=N");
  potion_method(aio_tty_vt, "reset_mode", aio_tty_reset_mode, 0);
  potion_method(aio_tty_vt, "get_winsize", aio_tty_get_winsize, 0);
  potion_method(aio_vt, "guess_handle", aio_guess_handle, "filefd=N");
  potion_method(aio_pipe_vt, "open", aio_pipe_open, "fd=N");
  potion_method(aio_pipe_vt, "bind", aio_pipe_bind, "name=S");
  potion_method(aio_pipe_vt, "connect", aio_pipe_connect, "req=o,name=S|connect_cb=&");
  potion_method(aio_pipe_vt, "start", aio_read2_start, "cb=&");
  potion_method(aio_pipe_vt, "stop", aio_read_stop, 0);
  potion_method(aio_pipe_vt, "write", aio_write2, "req=o,buf=O,bufcnt=N|write_cb=&");
  potion_method(aio_pipe_vt, "pending_instances", aio_pipe_pending_instances, "count=N");

  potion_method(aio_prepare_vt, "start", aio_prepare_start, "cb=&");
  potion_method(aio_prepare_vt, "stop", aio_prepare_stop, 0);
  potion_method(aio_check_vt, "start", aio_check_start, "cb=&");
  potion_method(aio_check_vt, "stop", aio_check_stop, 0);
  potion_method(aio_idle_vt, "start", aio_idle_start, "cb=&");
  potion_method(aio_idle_vt, "stop", aio_idle_stop, 0);
  potion_method(aio_async_vt, "send", aio_async_send, 0);
  potion_method(aio_timer_vt, "start", aio_timer_start, "cb=&,timeout=N,repeat=N");
  potion_method(aio_timer_vt, "stop", aio_timer_stop, 0);
  potion_method(aio_timer_vt, "again", aio_timer_again, 0);
  potion_method(aio_timer_vt, "set_repeat", aio_timer_set_repeat, "repeat=N");
  potion_method(aio_timer_vt, "repeat", aio_timer_get_repeat, 0);
  potion_method(aio_stream_vt, "listen", aio_listen, "backlog=N|cb=&");
  potion_method(aio_stream_vt, "accept", aio_accept, "client=o");
  potion_method(aio_stream_vt, "start", aio_read_start, "cb=&");
  potion_method(aio_stream_vt, "stop", aio_read_stop, 0);
  potion_method(aio_stream_vt, "readable", aio_is_readable, 0);
  potion_method(aio_stream_vt, "writable", aio_is_writable, 0);
  potion_method(aio_stream_vt, "closing", aio_is_closing, 0);
}
