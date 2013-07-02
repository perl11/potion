/** \file lib/aio.c
  asynch event-driven non-blocking IO, via libuv, on files, network streams, processes.
  \class Aio

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

PNType aio_type, aio_tcp_type, aio_udp_type, aio_loop_type, aio_tty_type, aio_pipe_type, /*aio_poll_type,*/ aio_prepare_type,
  aio_idle_type, aio_async_type, aio_timer_type, aio_fs_poll_type, aio_signal_type, aio_fs_event_type, aio_mutex_type,
  aio_rwlock_type, aio_sem_type, aio_cond_type, aio_barrier_type, aio_check_type,
  aio_handle_type, aio_process_type, aio_stream_type, aio_req_type, aio_connect_type, aio_write_type, aio_shutdown_type,
  aio_udp_send_type, aio_fs_type, aio_work_type, aio_getaddrinfo_type, aio_cpu_info_type, aio_interface_address_type;

//with wrapped callback
#define DEF_AIO_CB_WRAP(T) \
 DEF_AIO_CB_WRAP1(T,T,T)
#define DEF_AIO_CB_WRAP1(T,H,C)		\
typedef struct aio_##T##_s aio_##T##_t; \
struct aio_##T##_s {     \
  struct uv_##H##_s r;   \
  Potion *P;             \
  struct PNClosure *cb;	 \
}
//without
#define DEF_AIO_HANDLE_WRAP(T) \
typedef struct aio_##T##_s aio_##T##_t; \
struct aio_##T##_s {     \
  uv_##T##_t h;          \
  Potion *P;             \
  struct PNClosure *cb;	 \
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
DEF_AIO_CB_WRAP(req);
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
#undef DEF_AIO_CB_WRAP1

/**\memberof Aio_fs_event
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
  if (cb) cb->method(wrap->P, (PN)cb, (PN)data, potion_str(wrap->P, filename),
		     PN_NUM(events), PN_NUM(status));
}

static PN aio_last_error(Potion *P, char *name, uv_loop_t* loop) {
  uv_err_t err = uv_last_error(loop);
  return potion_error(P, potion_str_format(P, "Error %s: %s", name,
                                           uv_strerror(err)), 0, 0, 0);
}

#define DEF_AIO_NEW(T)                          \
  uv_##T##_t *handle;				\
  struct PNData * volatile data = potion_data_alloc(P, sizeof(aio_##T##_t)); \
  data->vt = aio_##T##_type;			\
  handle = (uv_##T##_t*)PN_DATA(data);          \
  ((aio_##T##_t*)handle)->P = P
#define DEF_AIO_NEW_LOOP(T)			\
  uv_loop_t* l;                                 \
  DEF_AIO_NEW(T);				\
  if (!loop) l = uv_default_loop();             \
  else if (PN_VTYPE(loop) == aio_loop_type)	\
    l = (uv_loop_t*)PN_DATA(loop);		\
  else return potion_type_error(P, loop);
#define DEF_AIO_NEW_LOOP_INIT(T)		 \
  DEF_AIO_NEW_LOOP(T);                           \
  if (uv_##T##_init(l, handle))			 \
    return aio_last_error(P, "Aio_"_XSTR(T), l); \
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

//TODO: check inheritence
#define CHECK_AIO_TYPE(self, T) \
  if (!potion_bind(P, self, PN_STR("Aio_"_XSTR(T)))) return potion_type_error_want(P, self, ""_XSTR(T))
//if (PN_VTYPE(self) != aio_##T##_type) return potion_type_error_want(P, self, ""_XSTR(T))
#define CHECK_AIO_STREAM(stream)                             \
  {                                                          \
    PNType _t = PN_VTYPE(stream);                            \
    if (_t != aio_stream_type &&                             \
        _t != aio_tcp_type &&                                \
        _t != aio_udp_type &&                                \
        _t != aio_pipe_type &&                               \
        _t != aio_tty_type &&                                \
        !potion_bind(P, stream, PN_STR("readable")))         \
      return potion_type_error_want(P, stream, "Aio_stream");\
  }
//if (PN_VTYPE(self) != aio_##T##_type) {
#define FATAL_AIO_TYPE(self, T) \
  if (!potion_bind(P, self, PN_STR("Aio_"_XSTR(T)))) {                  \
    fprintf(stderr, "** Invalid type %s, expected %s",                  \
            PN_IS_PTR(self)? AS_STR(potion_send(PN_VTABLE(self), PN_name)) \
            : PN_IS_NIL(self) ? NIL_NAME                                \
            : PN_IS_NUM(self) ? "Number" : "Boolean", "Aio_"_XSTR(T));  \
    exit(1);                                                            \
  }
#define FATAL_AIO_STREAM(stream)                             \
  {                                                          \
    PNType _t = PN_VTYPE(stream);			     \
    if (_t != aio_stream_type &&                             \
        _t != aio_tcp_type &&                                \
        _t != aio_udp_type &&                                \
        _t != aio_pipe_type &&                               \
        _t != aio_tty_type &&                                \
        !potion_bind(P, stream, PN_STR("readable")))         \
      {                                                      \
        fprintf(stderr, "** Invalid type %s, expected %s",   \
                PN_IS_PTR(stream)? AS_STR(potion_send(PN_VTABLE(stream), PN_name)) \
                : PN_IS_NIL(stream) ? NIL_NAME                            \
                : PN_IS_NUM(stream) ? "Number" : "Boolean", "Aio_stream");\
        exit(1);                                                          \
      }                                                                   \
  }

#define AIO_DATA(T,ARG) \
  (aio_##T##_t*)PN_DATA(potion_fwd(ARG)); \
  CHECK_AIO_TYPE(potion_fwd(ARG),T)
#define AIO_STREAM(ARG) \
  (aio_stream_t*)PN_DATA(potion_fwd(ARG)); \
  CHECK_AIO_STREAM(potion_fwd(ARG))

//cb wrappers
#define DEF_AIO_CB(T)				       \
  aio_##T##_t* wrap = (aio_##T##_t*)req;	       \
  vPN(Closure) cb = PN_CLOSURE(wrap->cb);              \
  PN data = (PN)((char*)wrap - sizeof(struct PNData)); \
  Potion *P = wrap->P;    \
  FATAL_AIO_TYPE(data,T); \
  if (cb) cb->method(P, (PN)cb, data, PN_NUM(status))

/**\class aio_tcp \memberof Lobby
   create and init a \c aio_tcp object
   \param loop   PNAioLoop to uv_loop_t*, defaults to uv_default_loop()
   \see http://nikhilm.github.io/uvbook/networking.html#tcp */
static PN aio_tcp_new(Potion *P, PN cl, PN self, PN loop) {
  DEF_AIO_NEW_LOOP_INIT(tcp);
  /*uv_loop_t* l;
  uv_tcp_t *handle;
  struct PNData * volatile data = potion_data_alloc(P, sizeof(aio_tcp_t));
  data->vt = aio_tcp_type;
  handle = (uv_tcp_t*)PN_DATA(data);
  ((aio_tcp_t*)handle)->P = P;
  if (!loop) l = uv_default_loop();
  else if (PN_VTYPE(loop) == aio_loop_type)
    l = (uv_loop_t*)PN_DATA(loop);
  else return potion_type_error(P, loop);
  if (uv_tcp_init(l, handle))
    return aio_last_error(P, "Aio_tcp", l);
  return (PN)data;*/
}
/**\class aio_udp \memberof Lobby
   create and init a \c aio_udp object
   \param loop   PNAioLoop to uv_loop_t*, defaults to uv_default_loop()
   \see http://nikhilm.github.io/uvbook/networking.html#udp */
static PN aio_udp_new(Potion *P, PN cl, PN self, PN loop) {
  //DEF_AIO_NEW_LOOP_INIT(udp);
  uv_loop_t* l;
  uv_udp_t *handle;
  struct PNData * volatile data = potion_data_alloc(P, sizeof(aio_udp_t));
  data->vt = aio_udp_type;
  handle = (uv_udp_t*)PN_DATA(data);
  ((aio_udp_t*)handle)->P = P;
  if (!loop) l = uv_default_loop();
  else if (PN_VTYPE(loop) == aio_loop_type)
    l = (uv_loop_t*)PN_DATA(loop);
  else return potion_type_error(P, loop);
  if (uv_udp_init(l, handle))
    return aio_last_error(P, "Aio_udp", l);
  //TODO: init ivars from struct
  return (PN)data;
}
/**\memberof Aio_udp
   get \c Aio_udp properties
   \param key PNString, One of "broadcast", "multicast_loop", "multicast_ttl", "ttl"
   \see http://nikhilm.github.io/uvbook/networking.html#udp */
static PN aio_udp_get(Potion *P, PN cl, PN self, PN key, PN value) {
  uv_udp_t *udp;
  udp = (uv_udp_t*)PN_DATA(potion_fwd(self));
  CHECK_AIO_TYPE(potion_fwd(self),udp);
  if (!potion_bind(P, key, PN_STR("eval")))
    return potion_type_error_want(P, key, "String");
  char *k = PN_STR_PTR(key);
  if (!strcmp(k, "broadcast")
   || !strcmp(k, "multicast_loop")
   || !strcmp(k, "multicast_ttl")
   || !strcmp(k, "ttl")
      ) {
    return potion_obj_get(P, 0, self, key);
  }
  else {
    return potion_error(P, potion_str_format(P, "Invalid key %s",
					     PN_STR_PTR(key)),
			0, 0, 0);
  }
}
/**\memberof Aio_udp
   set aio_udp properties
   \param key PNString, One of "broadcast", "multicast_loop", "multicast_ttl", "ttl"
   \param value
   \see http://nikhilm.github.io/uvbook/networking.html#udp */
static PN aio_udp_set(Potion *P, PN cl, PN self, PN key, PN value) {
  uv_udp_t *udp;
  udp = (uv_udp_t*)PN_DATA(potion_fwd(self));
  CHECK_AIO_TYPE(potion_fwd(self),udp);
  if (!potion_bind(P, key, PN_STR("eval")))
    return potion_type_error_want(P, key, "String");
  char *k = PN_STR_PTR(key);
  if (!strcmp(k, "broadcast")) {
    if (!PN_IS_BOOL(value))
      return potion_type_error_want(P, value, "Boolean");
    if (!uv_udp_set_broadcast(udp, value == PN_TRUE ? 1 : 0))
      potion_obj_set(P, 0, self, key, value);
  }
  else if (!strcmp(k, "multicast_loop")) {
    if (!PN_IS_BOOL(value))
      return potion_type_error_want(P, value, "Boolean");
    if (!uv_udp_set_multicast_loop(udp, value == PN_TRUE ? 1 : 0))
      potion_obj_set(P, 0, self, key, value);
  }
  else if (!strcmp(k, "multicast_ttl")) {
    if (!PN_IS_NUM(value))
      return potion_type_error_want(P, value, "Number");
    if (!uv_udp_set_multicast_ttl(udp, PN_INT(value)))
      potion_obj_set(P, 0, self, key, value);
  }
  else if (!strcmp(k, "ttl")) {
    if (!PN_IS_NUM(value))
      return potion_type_error_want(P, value, "Number");
    if (!uv_udp_set_ttl(udp, PN_INT(value)))
      potion_obj_set(P, 0, self, key, value);
  }
  else {
    return potion_error(P, potion_str_format(P, "Invalid key %s",
					     PN_STR_PTR(key)),
			0, 0, 0);
  }
  return (PN)self;
}
/**\class Aio_prepare \memberof Lobby
   create and init a \c Aio_prepare object
   Every active prepare handle gets its callback called exactly once per loop
   iteration, just before the system blocks to wait for completed i/o.
   \param loop Aio_loop, defaults to uv_default_loop()
   \see http://nikhilm.github.io/uvbook/networking.html#tcp */
static PN aio_prepare_new(Potion *P, PN cl, PN self, PN loop) {
  DEF_AIO_NEW_LOOP_INIT(prepare);
}
/**\class Aio_check \memberof Lobby
   create and init a \c Aio_check object
   \param loop Aio_loop, defaults to uv_default_loop() */
static PN aio_check_new(Potion *P, PN cl, PN self, PN loop) {
  DEF_AIO_NEW_LOOP_INIT(check);
}
/**\class aio_idle \memberof Lobby
   create and init a \c aio_idle object
   \param loop aio_loop, defaults to uv_default_loop() */
static PN aio_idle_new(Potion *P, PN cl, PN self, PN loop) {
  DEF_AIO_NEW_LOOP_INIT(idle);
}
/**\class aio_timer \memberof Lobby
   create and init a \c aio_timer object
   \param loop aio_loop, oop defaults to uv_default_loop()
   \see http://nikhilm.github.io/uvbook/utilities.html#timers */
static PN aio_timer_new(Potion *P, PN cl, PN self, PN loop) {
  DEF_AIO_NEW_LOOP_INIT(timer);
}
/**\class aio_fs_poll \memberof Lobby
   create and init a \c aio_fs_poll object
   \param loop aio_loop, oop defaults to uv_default_loop() */
static PN aio_fs_poll_new(Potion *P, PN cl, PN self, PN loop) {
  DEF_AIO_NEW_LOOP_INIT(fs_poll);
}
/**\class aio_signal \memberof Lobby
   create and init a \c aio_signal object
   \param loop aio_loop, oop defaults to uv_default_loop() */
static PN aio_signal_new(Potion *P, PN cl, PN self, PN loop) {
  DEF_AIO_NEW_LOOP_INIT(signal);
}
/**\class aio_loop \memberof Lobby
   create and init a \c aio_loop object, as uv_default_loop */
static PN aio_loop_new(Potion *P, PN cl, PN self) {
  uv_loop_t *l;
  uv_loop_t *def;
  struct PNData *data = potion_data_alloc(P,sizeof(aio_loop_t));
  ((struct aio_loop_s*)data)->P = P;
  l = (uv_loop_t*)PN_DATA(data);
  def = uv_default_loop();
  memcpy(l, def, sizeof(uv_loop_t));
  return (PN)data;
}
/**\class aio_handle \memberof Aio
   create a \c aio_handle */
static PN aio_handle_new(Potion *P, PN cl, PN self) {
  DEF_AIO_NEW(handle);
  return (PN)data;
}
/**\class aio_process \memberof Aio
   create a \c aio_stream */
static PN aio_stream_new(Potion *P, PN cl, PN self) {
  DEF_AIO_NEW(stream);
  return (PN)data;
}
/**\class aio_process \memberof Lobby
   create a \c aio_process */
static PN aio_process_new(Potion *P, PN cl, PN self) {
  DEF_AIO_NEW(process);
  return (PN)data;
}
/**\class aio_req \memberof Aio
   create a \c Aio_req */
static PN aio_req_new(Potion *P, PN cl, PN self) {
  DEF_AIO_NEW(req);
  return (PN)data;
}
/**\class aio_connect \memberof Aio_stream
   create a \c Aio_connect request */
static PN aio_connect_new(Potion *P, PN cl, PN self) {
  DEF_AIO_NEW(connect);
  return (PN)data;
}
/**\class aio_write \memberof Aio
   create a \c Aio_write request */
static PN aio_write_new(Potion *P, PN cl, PN self) {
  DEF_AIO_NEW(write);
  return (PN)data;
}
/**\class aio_shutdown \memberof Aio
   create a \c Aio_shutdown request */
static PN aio_shutdown_new(Potion *P, PN cl, PN self) {
  DEF_AIO_NEW(shutdown);
  return (PN)data;
}
/**\class aio_udp_send \memberof Aio_udp
   create a \c aio_udp_send request */
static PN aio_udp_send_new(Potion *P, PN cl, PN self) {
  DEF_AIO_NEW(udp_send);
  return (PN)data;
}
/**\class aio_fs \memberof Aio
   create a \c aio_fs filesystem request */
static PN aio_fs_new(Potion *P, PN cl, PN self) {
  DEF_AIO_NEW(fs);
  return (PN)data;
}
/**\class aio_work \memberof Aio
   create a \c aio_work request */
static PN aio_work_new(Potion *P, PN cl, PN self) {
  DEF_AIO_NEW(work);
  return (PN)data;
}
static PN aio_getaddrinfo_new(Potion *P, PN cl, PN self) {
  DEF_AIO_NEW(getaddrinfo);
  return (PN)data;
}
static PN aio_cpu_info_new(Potion *P, PN cl, PN self) {
  DEF_AIO_NEW(cpu_info);
  return (PN)data;
}
static PN aio_interface_address_new(Potion *P, PN cl, PN self) {
  DEF_AIO_NEW(interface_address);
  return (PN)data;
}
/**\class aio_tty \memberof Lobby
   create and init a \c aio_tty
   \param file FD INT ??
   \param readable INT
   \param loop (optional)
*/
static PN aio_tty_new(Potion *P, PN cl, PN self, PN file, PN readable, PN loop) {
  DEF_AIO_NEW_LOOP(tty);
  if (uv_tty_init(l, handle, PN_NUM(PN_DATA(file)), PN_NUM(readable)))
    return aio_last_error(P, "Aio_tty", l);
  return (PN)data;
}
/**\class aio_pipe \memberof Lobby
   create and init a \c aio_pipe
   \param ipc INT
   \param loop (optional)
*/
static PN aio_pipe_new(Potion *P, PN cl, PN self, PN ipc, PN loop) {
  DEF_AIO_NEW_LOOP(pipe);
  if (uv_pipe_init(l, handle, PN_NUM(ipc)))
    return aio_last_error(P, "Aio_pipe", l);
  return (PN)data;
}
/*static PN aio_poll_new(Potion *P, PN cl, PN self, PN fd, PN loop) {
  DEF_AIO_NEW_LOOP(poll);
  if (uv_poll_init((uv_loop_t*)loop, handle, PN_NUM(fd)))
    return aio_last_error(P, "Aio_poll", l);
  return (PN)data;
  }*/
/**\class aio_barrier \memberof Aio
   create and init a \c aio_barrier request
   \param count INT
*/
static PN aio_barrier_new(Potion *P, PN cl, PN self, PN count) {
  DEF_AIO_NEW(barrier);
  if (uv_barrier_init(handle, PN_NUM(count)))
    return potion_io_error(P, "Aio_barrier");
  return (PN)data;
}
/**\class aio_sem \memberof Aio
   create and init a \c aio_sem semaphore
   \param value INT
*/
static PN aio_sem_new(Potion *P, PN cl, PN self, PN value) {
  DEF_AIO_NEW(sem);
  if (uv_sem_init(handle, PN_NUM(value)))
    return potion_io_error(P, "Aio_sem");
  return (PN)data;
}
/**\class aio_fs_event \memberof Aio
   create and init a \c aio_fs_event
   \param filename PNString
   \param cb PNClosure or FFI function
   \param flags INT
*/
static PN aio_fs_event_new(Potion *P, PN cl, PN self, PN filename, PN cb, PN flags, PN loop) {
  DEF_AIO_NEW_LOOP(fs_event);
  uv_fs_event_cb fs_event_cb = aio_fs_event_cb;
  if (PN_IS_CLOSURE(cb)) handle->cb = (uv_fs_event_cb)cb;
  else if (PN_IS_FFIPTR(cb)) fs_event_cb = NULL;
  if (uv_fs_event_init(l, handle, PN_STR_PTR(filename), fs_event_cb, PN_NUM(flags)))
    return aio_last_error(P, "Aio_fs_event", l);
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
    return aio_last_error(P, "Aio_async", l);
  return (PN)data;
}
static PN aio_cond_new(Potion *P, PN cl, PN self) {
  DEF_AIO_NEW(cond);
  if (uv_cond_init(handle))
    return potion_io_error(P, "Aio_cond");
  return (PN)data;
}
static PN aio_mutex_new(Potion *P, PN cl, PN self) {
  DEF_AIO_NEW(mutex);
  if (uv_mutex_init(handle))
    return potion_io_error(P, "Aio_mutex");
  return (PN)data;
}
static PN aio_rwlock_new(Potion *P, PN cl, PN self) {
  DEF_AIO_NEW(rwlock);
  if (uv_rwlock_init(handle))
    return potion_io_error(P, "Aio_rwlock");
  return (PN)data;
}

#undef DEF_AIO_NEW
#undef DEF_AIO_NEW_LOOP

static void
aio_write_cb(uv_write_t* req, int status) {
  DEF_AIO_CB(write);
}
static void
aio_connect_cb(uv_connect_t* req, int status) {
  DEF_AIO_CB(connect);
/*aio_connect_t* wrap = (aio_connect_t*)req;
  vPN(Closure) cb = PN_CLOSURE(wrap->cb);
  PN data = (PN)((char*)wrap - sizeof(struct PNData));
  Potion *P = wrap->P;
  //FATAL_AIO_TYPE(data,connect);
  if (!potion_bind(P, data, PN_STR("Aio_connect"))) {
    fprintf(stderr, "** Invalid type %s, expected %s",
            PN_IS_PTR(data)? AS_STR(potion_send(PN_VTABLE(data), PN_name))
            : PN_IS_NIL(data) ? NIL_NAME
            : PN_IS_NUM(data) ? "Number" : "Boolean", "Aio_connect");
    exit(1);
  }
  if (cb) cb->method(P, (PN)cb, data, PN_NUM(status));*/
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
  PN data = (PN)((char*)wrap - sizeof(struct PNData));
  Potion *P = wrap->P;
  CHECK_AIO_TYPE(data,fs_poll);
  CHECK_AIO_TYPE(prev,stat);
  CHECK_AIO_TYPE(curr,stat);
  PN_CHECK_INT(status);
  if (cb) cb->method(P, (PN)cb, (PN)data, PN_NUM(status),
		     potion_ref(wrap->P, (PN)prev), potion_ref(wrap->P, (PN)curr));
}
#endif
static void
aio_read_cb(uv_stream_t* stream, ssize_t nread, uv_buf_t buf) {
  aio_stream_t* wrap = (aio_stream_t*)stream;
  vPN(Closure) cb = PN_CLOSURE(wrap->cb);
  PN data = (PN)((char*)wrap - sizeof(struct PNData));
  Potion *P = wrap->P;
  FATAL_AIO_STREAM(data);
  if (cb)
    cb->method(P, (PN)cb, (PN)data, PN_NUM(nread),
               potion_byte_str2(wrap->P, buf.base, buf.len));
}
static void
aio_read2_cb(uv_pipe_t* pipe, ssize_t nread, uv_buf_t buf, uv_handle_type pending) {
  aio_pipe_t* wrap = (aio_pipe_t*)pipe;
  vPN(Closure) cb = PN_CLOSURE(wrap->cb);
  PN data = (PN)((char*)wrap - sizeof(struct PNData));
  Potion *P = wrap->P;
  FATAL_AIO_TYPE(data, pipe);
  if (cb)
    cb->method(P, (PN)cb, (PN)data, PN_NUM(nread),
	       potion_byte_str2(wrap->P, buf.base, buf.len), PN_NUM(pending));
}
static void
aio_walk_cb(uv_handle_t* handle, void *arg) {
  aio_handle_t* wrap = (aio_handle_t*)handle;
  vPN(Closure) cb = PN_CLOSURE(wrap->cb);
  PN data = (PN)((char*)wrap - sizeof(struct PNData));
  Potion *P = wrap->P;
  FATAL_AIO_TYPE(data,handle);
  if (cb) cb->method(P, (PN)cb, (PN)data, 0);
}
static void
aio_close_cb(uv_handle_t* handle) {
  aio_handle_t* wrap = (aio_handle_t*)handle;
  vPN(Closure) cb = PN_CLOSURE(wrap->cb);
  PN data = (PN)((char*)wrap - sizeof(struct PNData));
  Potion *P = wrap->P;
  FATAL_AIO_TYPE(data,handle);
  if (cb) cb->method(P, (PN)cb, (PN)data);
}

/** \memberof Aio
 * Returns the libuv version packed into a single integer. 8 bits are used for
 * each component, with the patch number stored in the 8 least significant
 * bits. E.g. for libuv 1.2.3 this would return 0x010203. */
static PN
aio_version(Potion *P, PN cl, PN self) {
  return PN_NUM(uv_version());
}
/** \memberof Aio
 * Returns the libuv version number as a string. For non-release versions
 * "-pre" is appended, so the version number could be "1.2.3-pre". */
static PN
aio_version_string(Potion *P, PN cl, PN self) {
  return PN_STR(uv_version_string());
}
static PN aio_size(Potion *P, PN cl, PN self) {
  struct PNData* data = (struct PNData*)potion_fwd(self);
  return data->siz;
}

///\memberof Aio
static PN
aio_run(Potion *P, PN cl, PN self, PN loop, PN mode) {
  uv_loop_t* l;
  if (!loop) l = uv_default_loop();
  else if (PN_VTYPE(loop) == aio_loop_type)
    l = (uv_loop_t*)PN_DATA(loop);
  else return potion_type_error(P, loop);
  if (mode) PN_CHECK_TYPE(mode,PN_TNUMBER);
  return uv_run(l, mode ? (uv_run_mode)PN_INT(mode) : UV_RUN_DEFAULT)
    ? aio_last_error(P, "run", l) : self;
}
///\memberof Aio
/// Walk the list of open handles.
static PN
aio_walk(Potion *P, PN cl, PN self, PN loop, PN cb, PN arg) {
  uv_loop_t* l;
  if (!loop) l = uv_default_loop();
  else if (PN_VTYPE(loop) == aio_loop_type)
    l = (uv_loop_t*)PN_DATA(loop);
  else return potion_type_error(P, loop);

  AIO_CB_SET(walk,(aio_loop_t*)l);

  void *c_arg = PN_STR_PTR(arg);
  uv_walk(l, walk_cb, c_arg);
  return self;
}

///\memberof Aio_tcp
static PN
aio_tcp_open(Potion *P, PN cl, PN tcp, PN sock) {
  aio_tcp_t *handle = AIO_DATA(tcp,tcp);
  PN_CHECK_TYPE(sock,PN_TNUMBER);
  return uv_tcp_open(&handle->r, PN_INT(sock))
    ? aio_last_error(P, "tcp open", handle->r.loop) : tcp;
}
///\memberof Aio_tcp
static PN
aio_tcp_nodelay(Potion *P, PN cl, PN tcp, PN enable) {
  aio_tcp_t *handle = AIO_DATA(tcp,tcp);
  PN_CHECK_TYPE(enable,PN_TNUMBER);
  return uv_tcp_nodelay(&handle->r, PN_INT(enable))
    ? aio_last_error(P, "tcp nodelay", handle->r.loop) : tcp;
}
///\memberof Aio_tcp
static PN
aio_tcp_keepalive(Potion *P, PN cl, PN tcp, PN enable, PN delay) {
  aio_tcp_t *handle = AIO_DATA(tcp,tcp);
  PN_CHECK_TYPE(enable,PN_TNUMBER);
  PN_CHECK_TYPE(delay,PN_TNUMBER);
  return uv_tcp_keepalive(&handle->r, PN_INT(enable), PN_INT(delay))
    ? aio_last_error(P, "tcp keepalive", handle->r.loop) : tcp;
}
///\memberof Aio_tcp
static PN
aio_tcp_simultaneous_accepts(Potion *P, PN cl, PN tcp, PN enable) {
  aio_tcp_t *handle = AIO_DATA(tcp,tcp);
  PN_CHECK_INT(enable); //TODO bool
  return uv_tcp_simultaneous_accepts(&handle->r, PN_INT(enable))
    ? aio_last_error(P, "tcp simultaneous_accepts", handle->r.loop) : tcp;
}
///\memberof Aio_tcp
static PN
aio_tcp_bind(Potion *P, PN cl, PN tcp, PN addr, PN port) {
  aio_tcp_t *handle = AIO_DATA(tcp,tcp);
  PN_CHECK_STR(addr);
  PN_CHECK_INT(port);
  struct sockaddr_in ip4 = uv_ip4_addr(PN_STR_PTR(addr), PN_INT(port));
  return uv_tcp_bind(&handle->r, ip4)
    ? aio_last_error(P, "tcp bind", handle->r.loop) : tcp;
}
///\memberof Aio_tcp
static PN
aio_tcp_bind6(Potion *P, PN cl, PN tcp, PN addr, PN port) {
  aio_tcp_t *handle = AIO_DATA(tcp,tcp);
  PN_CHECK_STR(addr);
  PN_CHECK_INT(port);
  struct sockaddr_in6 ip6 = uv_ip6_addr(PN_STR_PTR(addr), PN_INT(port));
  return uv_tcp_bind6(&handle->r, ip6)
    ? aio_last_error(P, "tcp bind6", handle->r.loop) : tcp;
}
///\memberof Aio_tcp
static PN
aio_tcp_getsockname(Potion *P, PN cl, PN tcp) {
  struct sockaddr sock; int len;
  aio_tcp_t *handle = AIO_DATA(tcp,tcp);
  return (uv_tcp_getsockname(&handle->r, &sock, &len))
    ? aio_last_error(P, "tcp getsockname", handle->r.loop)
    : potion_str2(P, sock.sa_data, len);
}
///\memberof Aio_tcp
static PN
aio_tcp_getpeername(Potion *P, PN cl, PN tcp) {
  struct sockaddr sock; int len;
  aio_tcp_t *handle = AIO_DATA(tcp,tcp);
  return (uv_tcp_getpeername(&handle->r, &sock, &len))
    ? aio_last_error(P, "tcp getpeername", handle->r.loop)
    : potion_str2(P, sock.sa_data, len);
}
///\memberof Aio_tcp
static PN
aio_tcp_connect(Potion *P, PN cl, PN tcp, PN req, PN addr, PN port, PN cb) {
  aio_tcp_t *handle = AIO_DATA(tcp,tcp);
  aio_connect_t *request = (aio_connect_t*)PN_DATA(potion_fwd(req));
  PN_CHECK_STR(addr);
  PN_CHECK_INT(port);
  struct sockaddr_in ip4 = uv_ip4_addr(PN_STR_PTR(addr), PN_INT(port));
  AIO_CB_SET(connect,request);
  return uv_tcp_connect(&request->r, &handle->r, ip4, connect_cb)
    ? aio_last_error(P, "tcp connect", handle->r.loop)
    : tcp;
}
///\memberof Aio_tcp
static PN
aio_tcp_connect6(Potion *P, PN cl, PN tcp, PN req, PN addr, PN port, PN cb) {
  aio_tcp_t *handle = AIO_DATA(tcp,tcp);
  aio_connect_t *request = (aio_connect_t*)PN_DATA(potion_fwd(req));
  PN_CHECK_STR(addr);
  PN_CHECK_INT(port);
  struct sockaddr_in6 ip6 = uv_ip6_addr(PN_STR_PTR(addr), PN_INT(port));
  AIO_CB_SET(connect,request);
  return uv_tcp_connect6(&request->r, &handle->r, ip6, connect_cb)
    ? aio_last_error(P, "tcp connect6", handle->r.loop)
    : tcp;
}
///\memberof Aio_udp
///\returns aio_udp or nil
static PN
aio_udp_open(Potion *P, PN cl, PN udp, PN sockfd) {
  aio_udp_t *handle = AIO_DATA(udp,udp);
  PN_CHECK_INT(sockfd);
  return uv_udp_open(&handle->r, PN_INT(sockfd))
    ? aio_last_error(P, "udp open", handle->r.loop)
    : udp;
}
///\memberof Aio_udp
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
///\memberof Aio_udp
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
///\memberof Aio_udp
static PN
aio_udp_getsockname(Potion *P, PN cl, PN udp) {
  struct sockaddr sock; int len;
  aio_udp_t *handle = AIO_DATA(udp,udp);
  return uv_udp_getsockname(&handle->r, &sock, &len)
    ? aio_last_error(P, "udp getsockname", handle->r.loop)
    : potion_str2(P, sock.sa_data, len);
}
///\memberof Aio_udp
///TODO: setpath handler
static PN
aio_udp_set_membership(Potion *P, PN cl, PN udp, PN mcaddr, PN ifaddr, PN membership) {
  aio_udp_t *handle = AIO_DATA(udp,udp);
  PN_CHECK_STR(mcaddr);
  PN_CHECK_STR(ifaddr);
  PN_CHECK_INT(membership);
  return uv_udp_set_membership(&handle->r, PN_STR_PTR(mcaddr), PN_STR_PTR(ifaddr),
                               (uv_membership)PN_NUM(membership))
    ? aio_last_error(P, "udp set_membership", handle->r.loop)
    : udp;
}
///\memberof Aio_udp
static PN
aio_udp_set_multicast_loop(Potion *P, PN cl, PN udp, PN on) {
  aio_udp_t *handle = AIO_DATA(udp,udp);
  PN_CHECK_INT(on);  //TODO boolean
  return uv_udp_set_multicast_loop(&handle->r, PN_NUM(on))
    ? aio_last_error(P, "udp set_multicast_loop", handle->r.loop)
    : udp;
}
///\memberof Aio_udp
static PN
aio_udp_set_multicast_ttl(Potion *P, PN cl, PN udp, PN ttl) {
  aio_udp_t *handle = AIO_DATA(udp,udp);
  PN_CHECK_INT(ttl);
  return uv_udp_set_multicast_ttl(&handle->r, PN_NUM(ttl))
    ? aio_last_error(P, "udp set_multicast_ttl", handle->r.loop)
    : udp;
}
///\memberof Aio_udp
static PN
aio_udp_set_broadcast(Potion *P, PN cl, PN udp, PN on) {
  aio_udp_t *handle = AIO_DATA(udp,udp);
  PN_CHECK_INT(on);  //TODO boolean
  return uv_udp_set_broadcast(&handle->r, PN_NUM(on))
    ? aio_last_error(P, "udp set_broadcast", handle->r.loop)
    : udp;
}
///\memberof Aio_udp
static PN
aio_udp_set_ttl(Potion *P, PN cl, PN udp, PN ttl) {
  aio_udp_t *handle = AIO_DATA(udp,udp);
  PN_CHECK_INT(ttl);
  return uv_udp_set_ttl(&handle->r, PN_NUM(ttl))
    ? aio_last_error(P, "udp set_ttl", handle->r.loop)
    : udp;
}

///\memberof Aio_udp
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
///\memberof Aio_udp
static PN
aio_udp_send6(Potion *P, PN cl, PN udp, PN req, PN buf, PN bufcnt, PN addr, PN port, PN cb) {
  aio_udp_t *handle = AIO_DATA(udp,udp);
  aio_udp_send_t *request = (aio_udp_send_t*)PN_DATA(potion_fwd(req));
  //aio_udp_send_t *request = AIO_DATA(udp_send, req);
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

/**\memberof Aio_udp
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
  char *data = (char*)(wrap - sizeof(struct PNData));
  if (addr_in->sin_family == AF_INET6) {
    uv_ip6_name((struct sockaddr_in6*)addr, ip, 46);
    port = ((struct sockaddr_in6*)addr)->sin6_port;
  } else {
    uv_ip4_name(addr_in, ip, 46);
    port = addr_in->sin_port;
  }
  //TODO: maybe reuse PNBytes with buf structure
  if (cb)
    cb->method(wrap->P, (PN)cb, (PN)data, PN_NUM(nread),
	       potion_byte_str2(wrap->P, buf.base, buf.len),
	       potion_byte_str(wrap->P, ip),
	       PN_NUM(port), PN_NUM(flags));
}

///\memberof Aio_udp
static PN
aio_udp_recv_start(Potion *P, PN cl, PN udp, PN cb) {
  aio_udp_t *handle = AIO_DATA(udp,udp);
  AIO_CB_SET(udp_recv,handle);
  return uv_udp_recv_start(&handle->r, aio_alloc_cb, udp_recv_cb)
    ? aio_last_error(P, "udp recv_start", handle->r.loop)
    : udp;
}
///\memberof Aio_udp
static PN
aio_udp_recv_stop(Potion *P, PN cl, PN udp) {
  aio_udp_t *handle = AIO_DATA(udp,udp);
  return uv_udp_recv_stop(&handle->r)
    ? aio_last_error(P, "udp recv_stop", handle->r.loop)
    : udp;
}
///\memberof Aio_prepare
static PN
aio_prepare_start(Potion *P, PN cl, PN self, PN cb) {
  aio_prepare_t *handle = AIO_DATA(prepare,self);
  AIO_CB_SET(prepare,handle);
  return uv_prepare_start(&handle->r, prepare_cb)
    ? aio_last_error(P, "prepare start", handle->r.loop)
    : self;
}
///\memberof Aio_prepare
static PN
aio_prepare_stop(Potion *P, PN cl, PN self) {
  aio_prepare_t *handle = AIO_DATA(prepare,self);
  return uv_prepare_stop(&handle->r)
    ? aio_last_error(P, "prepare stop", handle->r.loop)
    : self;
}
///\memberof Aio_check
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
///\memberof Aio_idle
static PN
aio_idle_start(Potion *P, PN cl, PN self, PN cb) {
  aio_idle_t *handle = AIO_DATA(idle, self);
  AIO_CB_SET(idle,handle);
  return uv_idle_start(&handle->r, idle_cb)
    ? aio_last_error(P, "idle start", handle->r.loop)
    : self;
}
///\memberof Aio_idle
static PN
aio_idle_stop(Potion *P, PN cl, PN self) {
  aio_idle_t *handle = AIO_DATA(idle, self);
  return uv_idle_stop(&handle->r)
    ? aio_last_error(P, "idle stop", handle->r.loop)
    : self;
}

///\memberof Aio_tty
/// Set mode. 0 for normal, 1 for raw.
static PN
aio_tty_set_mode(Potion *P, PN cl, PN tty, PN mode) {
  aio_tty_t *handle = AIO_DATA(tty,tty);
  PN_CHECK_INT(mode);
  return uv_tty_set_mode(&handle->h, PN_INT(mode))
    ? aio_last_error(P, "tty set_mode", handle->h.loop)
    : tty;
}
///\memberof Aio_tty
/// To be called when the program exits. Resets TTY settings to default
/// values for the next process to take over.
static PN
aio_tty_reset_mode(Potion *P, PN cl, PN tty) {
  uv_tty_reset_mode();
  return tty;
}
///\memberof Aio_tty
static PN
aio_tty_get_winsize(Potion *P, PN cl, PN tty) {
  aio_tty_t *handle = AIO_DATA(tty,tty);
  int width, height;
  if (uv_tty_get_winsize(&handle->h, &width, &height))
    return aio_last_error(P, "tty get_winsize", handle->h.loop);
  else
    return PN_PUSH(PN_PUSH(PN_TUP0(), PN_NUM(width)), PN_NUM(height));
}
///\memberof Aio
static PN
aio_guess_handle(Potion *P, PN cl, PN file) {
  PN_CHECK_INT(file);
  uv_handle_type r = uv_guess_handle(PN_INT(file));
  return PN_NUM(r);
}

///\memberof Aio_pipe
static PN
aio_pipe_open(Potion *P, PN cl, PN pipe, PN file) {
  aio_pipe_t *handle = AIO_DATA(pipe,pipe);
  return uv_pipe_open(&handle->r, PN_INT(file))
    ? aio_last_error(P, "pipe open", handle->r.loop)
    : pipe;
}
///\memberof Aio_pipe
static PN
aio_pipe_bind(Potion *P, PN cl, PN pipe, PN name) {
  aio_pipe_t *handle = AIO_DATA(pipe,pipe);
  PN_CHECK_STR(name);
  return uv_pipe_bind(&handle->r, PN_STR_PTR(name))
    ? aio_last_error(P, "pipe bind", handle->r.loop)
    : pipe;
}
///\memberof Aio_pipe
static PN
aio_pipe_connect(Potion *P, PN cl, PN pipe, PN req, PN name, PN cb) {
  aio_pipe_t *handle = AIO_DATA(pipe,pipe);
  aio_connect_t *request = AIO_DATA(connect,req);
  AIO_CB_SET(connect,request);
  PN_CHECK_STR(name);
  uv_pipe_connect(&request->r, &handle->r, PN_STR_PTR(name), connect_cb);
  return pipe;
}
///\memberof Aio_pipe
// Windows only
static PN
aio_pipe_pending_instances(Potion *P, PN cl, PN pipe, PN count) {
  aio_pipe_t *handle = AIO_DATA(pipe,pipe);
  PN_CHECK_INT(count);
  uv_pipe_pending_instances(&handle->r, PN_INT(count));
  return pipe;
}
///\memberof Aio_stream
static PN
aio_shutdown(Potion *P, PN cl, PN stream, PN req, PN cb) {
  aio_stream_t *handle = AIO_DATA(stream,stream);
  aio_shutdown_t *request = AIO_DATA(shutdown,req);
  AIO_CB_SET(shutdown,request);
  return uv_shutdown(&request->r, &handle->r, shutdown_cb)
    ? aio_last_error(P, "shutdown", handle->r.loop)
    : stream;
}

/**\memberof Aio_stream
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
///\memberof Aio_stream
static PN
aio_listen(Potion *P, PN cl, PN stream, PN backlog, PN cb) {
  aio_stream_t *request = AIO_STREAM(stream);
  AIO_CB_SET(connection,request);
  PN_CHECK_INT(backlog);
  return uv_listen(&request->r, PN_INT(backlog), connection_cb)
    ? aio_last_error(P, "listen", request->r.loop)
    : stream;
}
/** \memberof Aio_stream
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
/** \memberof Aio_stream
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
///\memberof Aio_stream
static PN
aio_read_stop(Potion *P, PN cl, PN self) {
  aio_stream_t *handle = AIO_STREAM(self);
  return uv_read_stop(&handle->r)
    ? aio_last_error(P, "read stop", handle->r.loop)
    : self;
}
///\memberof Aio_pipe
static PN
aio_read2_start(Potion *P, PN cl, PN self, PN cb) {
  aio_pipe_t *handle = AIO_DATA(pipe,self);
  AIO_CB_SET(read2,handle);
  return uv_read2_start((uv_stream_t*)&handle->r, aio_alloc_cb, read2_cb)
    ? aio_last_error(P, "read2 start", handle->r.loop)
    : self;
}
/** \memberof Aio_stream
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
/** \memberof Aio_stream
    \returns PNBoolean */
static PN
aio_is_readable(Potion *P, PN cl, PN stream) {
  const aio_stream_t *handle = AIO_STREAM(stream);
  return uv_is_readable(&handle->r) ? PN_TRUE : PN_FALSE;
}
/** \memberof Aio_stream
    \returns PNBoolean */
static PN
aio_is_writable(Potion *P, PN cl, PN stream) {
  const aio_stream_t *handle = AIO_STREAM(stream);
  return uv_is_writable(&handle->r) ? PN_TRUE : PN_FALSE;
}
/** \memberof Aio_stream
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
/** \memberof Aio_handle
 * Returns TRUE if the prepare/check/idle/timer handle has been started, FALSE
 * otherwise. For other handle types this always returns TRUE.
 * \returns PNBoolean */
static PN
aio_is_active(Potion *P, PN cl, PN self) {
  const aio_handle_t *handle = AIO_DATA(handle,self);
  return uv_is_active((const uv_handle_t*)&handle->h) ? PN_TRUE : PN_FALSE;
}

/**\memberof Aio_async
 * This can be called from other threads to wake up a libuv thread.
 * aio/libuv is single threaded at the moment. */
static PN
aio_async_send(Potion *P, PN cl, PN self) {
  aio_async_t *handle = AIO_DATA(async,self);
  return PN_NUM(uv_async_send(&handle->r));
}
///\memberof Aio_timer
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
///\memberof Aio_timer
static PN
aio_timer_stop(Potion *P, PN cl, PN self) {
  aio_timer_t *handle = AIO_DATA(timer,self);
  return uv_timer_stop(&handle->r)
    ? aio_last_error(P, "timer stop", handle->r.loop)
    : self;
}
/** \memberof Aio_timer
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
///\memberof Aio_timer
//TODO get/set as object ivar
static PN
aio_timer_get_repeat(Potion *P, PN cl, PN self) {
  aio_timer_t *handle = AIO_DATA(timer,self);
  return PN_NUM(uv_timer_get_repeat(&handle->r));
}
/**\memberof Aio_timer
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
/**\memberof Aio_handle
 * Returns size of various libuv handle types, useful for FFI
 * bindings to allocate correct memory without copying struct
 * definitions.
 */
static PN
aio_handle_uvsize(Potion *P, PN cl, PN self) {
  aio_handle_t *handle = AIO_DATA(handle,self);
  return PN_NUM(uv_handle_size(handle->h.type));
}
/**\memberof Aio_req
 * Returns size of various libuv request types, useful for dynamic lookup with FFI.
 */
static PN
aio_req_uvsize(Potion *P, PN cl, PN self) {
  aio_req_t *req = AIO_DATA(req,self);
  return PN_NUM(uv_req_size(req->r.type));
}

/**\memberof Aio_handle
 * Request handle to be closed. close_cb will be called asynchronously after
 * this call. This MUST be called on each handle before memory is released.
 *
 * Note that handles that wrap file descriptors are closed immediately but
 * close_cb will still be deferred to the next iteration of the event loop.
 * It gives you a chance to free up any resources associated with the handle.
 *
 * In-progress requests, like uv_connect_t or uv_write_t, are cancelled and
 * have their callbacks called asynchronously with status=-1 and the error code
 * set to UV_ECANCELED. */
static PN
aio_close(Potion *P, PN cl, PN self, PN cb) {
  aio_handle_t* handle = AIO_DATA(handle,self);
  AIO_CB_SET(close, handle);
  uv_close(&handle->h, close_cb);
  return self;
}


#undef AIO_CB_SET

void Potion_Init_aio(Potion *P) {
  PN aio_vt = potion_class(P, 0, 0, 0);
  aio_type = potion_class_type(P, aio_vt);
  potion_define_global(P, PN_STR("Aio"), aio_vt);
#define DEF_AIO__VT(T,paren) \
  PN aio_##T##_vt = potion_class(P, 0, paren##_vt, 0);	\
  aio_##T##_type = potion_class_type(P, aio_##T##_vt); \
  potion_define_global(P, PN_STR("Aio_" _XSTR(T)), aio_##T##_vt); \
  potion_method(aio_##T##_vt, ""_XSTR(T), aio_##T##_new, 0)
#define DEF_AIO_CTOR(T) \
  potion_type_constructor_is(aio_##T##_vt, PN_FUNC(aio_##T##_new, 0));
#define DEF_AIO_VT(T,paren) \
  PN aio_##T##_vt = potion_class(P, 0, paren##_vt, 0);	\
  aio_##T##_type = potion_class_type(P, aio_##T##_vt); \
  DEF_AIO_CTOR(T); \
  potion_define_global(P, PN_STR("Aio_" _XSTR(T)), aio_##T##_vt); \
  potion_method(aio_##T##_vt, ""_XSTR(T), aio_##T##_new, 0)
#define DEF_AIO_GLOBAL_VT(T,paren,args) \
  DEF_AIO__VT(T,paren); \
  potion_type_call_is(aio_##T##_vt, PN_FUNC(aio_##T##_new, args)); \
  potion_type_constructor_is(aio_##T##_vt, PN_FUNC(aio_##T##_new, args)); \
  potion_method(P->lobby, "Aio_"_XSTR(T), aio_##T##_new, args)

  potion_define_global(P, PN_STR("AIO_RUN_DEFAULT"), PN_NUM(0));
  potion_define_global(P, PN_STR("AIO_RUN_ONCE"),   PN_NUM(1));
  potion_define_global(P, PN_STR("AIO_RUN_NOWAIT"), PN_NUM(2));
  potion_define_global(P, PN_STR("AIO_UDP_IPV6ONLY"), PN_NUM(1));
  potion_define_global(P, PN_STR("AIO_UDP_PARTIAL"), PN_NUM(2));
  potion_method(P->lobby, "Aio_version", aio_version, 0);
  potion_method(P->lobby, "Aio_version_string", aio_version_string, 0);
  potion_method(aio_vt, "version", aio_version, 0);
  potion_method(aio_vt, "version_string", aio_version_string, 0);

  DEF_AIO_VT(handle,aio);
  DEF_AIO_VT(stream,aio);
  DEF_AIO_GLOBAL_VT(tcp,aio_stream,"|loop=o");
  //DEF_AIO_GLOBAL_VT(udp,aio_stream,"|loop=o");
  PN udp_ivars = potion_tuple_with_size(P, 6); //sorted list of path names
  vPN(Tuple) t = PN_GET_TUPLE(udp_ivars);
  t->set[0] = PN_STR("broadcast");
  t->set[1] = PN_STR("multicast_loop");
  t->set[2] = PN_STR("multicast_ttl");
  t->set[3] = PN_STR("ttl");
  PN aio_udp_vt = potion_class(P, 0, aio_stream_vt, udp_ivars);
  aio_udp_type = potion_class_type(P, aio_udp_vt);
  potion_define_global(P, PN_STR("Aio_udp"), aio_udp_vt);
  //potion_method(aio_udp_vt, "udp", aio_udp_new, 0);
  potion_type_call_is(aio_udp_vt, PN_FUNC(aio_udp_get, "key=S"));
  potion_type_callset_is(aio_udp_vt, PN_FUNC(aio_udp_set, "key=S,value=o"));
  potion_type_constructor_is(aio_udp_vt, PN_FUNC(aio_udp_new, "|loop=o"));
  potion_method(P->lobby, "Aio_udp", aio_udp_new, "|loop=o");

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
  //tcp,udp have connect so we need a global Aio_connect method
  DEF_AIO_GLOBAL_VT(connect,aio_stream,0);
  DEF_AIO_VT(req,aio);
  DEF_AIO_VT(write,aio);
  DEF_AIO_VT(shutdown,aio);
  DEF_AIO_VT(udp_send,aio_udp);
  DEF_AIO_VT(fs,aio);
  DEF_AIO_VT(work,aio);
  DEF_AIO_VT(getaddrinfo,aio);
  DEF_AIO_VT(cpu_info,aio);
  DEF_AIO_VT(interface_address,aio);

#undef DEF_AIO_VT_1
#undef DEF_AIO_CTOR
#undef DEF_AIO_VT
#undef DEF_AIO_GLOBAL_VT

  potion_method(aio_vt, "size", aio_size, 0);
  potion_method(aio_req_vt, "uvsize", aio_req_uvsize, 0);
  potion_method(aio_handle_vt, "uvsize", aio_handle_uvsize, 0);
  potion_method(aio_handle_vt, "active?", aio_is_active, 0);
  potion_method(aio_handle_vt, "close", aio_close, "|cb=&");
  potion_method(aio_vt, "walk", aio_walk, "|loop=o,fun=&,arg=o");
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
