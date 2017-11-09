#include <libudev.h>
#include <stdio.h>
#include <stdlib.h>
#include <locale.h>
#include <unistd.h>
#include "erl_nif.h"

/*#define DEBUG*/

#define ASSERT(e) ((void)((e) ? 1 : (assert_error(#e, __func__, __FILE__, __LINE__), 0)))

static void
assert_error(const char *expr, const char *func, const char *file, int line)
{
    fflush(stdout);
    fprintf(stderr, "%s:%d:%s() Assertion failed: %s\n", file, line, func, expr);
    fflush(stderr);
    abort();
}

typedef struct {
  struct udev *udev;
  struct udev_monitor *monitor;
  int fd;
} Monitor;

static void
mon_rt_dtor(ErlNifEnv *env, void *obj)
{
  enif_fprintf(stderr, "mon_rt_dtor called\n");
}

static void
mon_rt_stop(ErlNifEnv *env, void *obj, int fd, int is_direct_call)
{
  enif_fprintf(stderr, "mon_rt_stop called %s\n", (is_direct_call ? "DIRECT" : "LATER"));
}

static ErlNifResourceType *mon_rt;
static ERL_NIF_TERM atom_ok;
static ERL_NIF_TERM atom_undefined;

static ErlNifResourceTypeInit mon_rt_init = {mon_rt_dtor, mon_rt_stop};

static int
load(ErlNifEnv* env, void** priv, ERL_NIF_TERM info) {
  atom_ok = enif_make_atom(env, "ok");
  atom_undefined = enif_make_atom(env, "undefined");

  mon_rt = enif_open_resource_type_x(env, "monitor", &mon_rt_init, ERL_NIF_RT_CREATE, NULL);

  return !mon_rt;
}

static int
reload(ErlNifEnv* env, void** priv, ERL_NIF_TERM info) {
  return 0;
}

static int
upgrade(ErlNifEnv* env, void** priv, void** old_priv, ERL_NIF_TERM info) {
  return load(env, priv, info);
}

static void
unload(ErlNifEnv* env, void* priv) {
  enif_free(priv);
}

static ERL_NIF_TERM
start(ErlNifEnv *env, int argc, const ERL_NIF_TERM argv[])
{
  Monitor *mon;
  ERL_NIF_TERM res;
  int fd;
	struct udev *udev;
  struct udev_monitor *udev_mon;

  udev = udev_new();
  if(!udev) {
    enif_fprintf(stderr, "Can't make udev\n");
    return enif_make_badarg(env);
  }

  udev_mon = udev_monitor_new_from_netlink(udev, "udev");
	udev_monitor_enable_receiving(udev_mon);
	fd = udev_monitor_get_fd(udev_mon);

  mon = enif_alloc_resource(mon_rt, sizeof(Monitor));
  mon->udev = udev;
  mon->monitor = udev_mon;
  mon->fd = fd;

  res = enif_make_resource(env, mon);
  enif_release_resource(mon);

  return enif_make_tuple2(env, atom_ok, res);
}

static ERL_NIF_TERM
stop(ErlNifEnv *env, int argc, const ERL_NIF_TERM argv[])
{
  Monitor *mon;
  int rv;

  if(!enif_get_resource(env, argv[0], mon_rt, (void **)&mon)) {
    return enif_make_badarg(env);
  }

  enif_fprintf(stderr, "Closing fd=%d\n", mon->fd);

  rv = enif_select(env, mon->fd, ERL_NIF_SELECT_STOP, mon, NULL, atom_undefined);
  ASSERT(rv >= 0);

  return atom_ok;
}

static ErlNifFunc nif_funcs[] = {
  {"start", 0, start},
  {"stop", 1, stop}
};

ERL_NIF_INIT(Elixir.Udev, nif_funcs, &load, &reload, &upgrade, &unload)
