#include <libudev.h>
#include <stdio.h>
#include <stdlib.h>
#include <locale.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include "erl_nif.h"

/*#define DEBUG*/

#define ASSERT(e) ((void)((e) ? 1 : (assert_error(#e, __func__, __FILE__, __LINE__), 0)))
#define SET_NONBLOCKING(fd) fcntl((fd), F_SETFL, fcntl((fd), F_GETFL, 0) | O_NONBLOCK)

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
  Monitor *mon = (Monitor *)obj;
  enif_fprintf(stderr, "mon_rt_stop called %s\n", (is_direct_call ? "DIRECT" : "LATER"));

  udev_unref(mon->udev);
}

static ErlNifResourceType *mon_rt;
static ERL_NIF_TERM atom_ok;
static ERL_NIF_TERM atom_undefined;
static ERL_NIF_TERM atom_error;
static ERL_NIF_TERM atom_node;
static ERL_NIF_TERM atom_subsystem;
static ERL_NIF_TERM atom_devtype;
static ERL_NIF_TERM atom_action;

static ErlNifResourceTypeInit mon_rt_init = {mon_rt_dtor, mon_rt_stop};

static int
load(ErlNifEnv* env, void** priv, ERL_NIF_TERM info) {
  atom_ok = enif_make_atom(env, "ok");
  atom_undefined = enif_make_atom(env, "undefined");
  atom_error = enif_make_atom(env, "error");
  atom_node = enif_make_atom(env, "node");
  atom_subsystem = enif_make_atom(env, "subsystem");
  atom_devtype = enif_make_atom(env, "devtype");
  atom_action = enif_make_atom(env, "action");

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

  SET_NONBLOCKING(fd);

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

static ERL_NIF_TERM
poll(ErlNifEnv *env, int argc, const ERL_NIF_TERM argv[])
{
  Monitor *mon;
  int rv;

  if(!enif_get_resource(env, argv[0], mon_rt, (void **)&mon)) {
    return enif_make_badarg(env);
  }

  enif_fprintf(stderr, "Polling fd=%d\n", mon->fd);

  rv = enif_select(env, mon->fd, ERL_NIF_SELECT_READ, mon, NULL, atom_undefined);
  ASSERT(rv >= 0);

  return atom_ok;
}

static ERL_NIF_TERM
receive_device(ErlNifEnv *env, int argc, const ERL_NIF_TERM argv[])
{
  Monitor *mon;
  struct udev_device *dev;
  const char *devnode, *subsystem, *devtype, *action;
  ErlNifBinary devnode_bin, subsystem_bin, devtype_bin, action_bin;

  if(!enif_get_resource(env, argv[0], mon_rt, (void **)&mon)) {
    return enif_make_badarg(env);
  }

  enif_fprintf(stderr, "Reading fd=%d\n", mon->fd);

  dev = udev_monitor_receive_device(mon->monitor);

  if(dev) {
    ERL_NIF_TERM map = enif_make_new_map(env);

    devnode = udev_device_get_devnode(dev);
    if(devnode == NULL)
      devnode = "";
    enif_fprintf(stderr, "devnode=%s\n", devnode);

    enif_alloc_binary(strlen(devnode), &devnode_bin);
    memcpy(devnode_bin.data, devnode, strlen(devnode));

    subsystem = udev_device_get_subsystem(dev);
    if(subsystem == NULL)
      subsystem = "";
    enif_fprintf(stderr, "subsystem=%s\n", subsystem);

    enif_alloc_binary(strlen(subsystem), &subsystem_bin);
    memcpy(subsystem_bin.data, subsystem, strlen(subsystem));

    devtype = udev_device_get_devtype(dev);
    if(devtype == NULL)
      devtype = "";
    enif_fprintf(stderr, "devtype=%s\n", devtype);

    enif_alloc_binary(strlen(devtype), &devtype_bin);
    memcpy(devtype_bin.data, devtype, strlen(devtype));

    action = udev_device_get_action(dev);
    if(action == NULL)
      action = "";
    enif_fprintf(stderr, "action=%s\n", action);

    enif_alloc_binary(strlen(action), &action_bin);
    memcpy(action_bin.data, action, strlen(action));

    enif_make_map_put(env, map, atom_node, enif_make_binary(env, &devnode_bin), &map);
    enif_make_map_put(env, map, atom_subsystem, enif_make_binary(env, &subsystem_bin), &map);
    enif_make_map_put(env, map, atom_devtype, enif_make_binary(env, &devtype_bin), &map);
    enif_make_map_put(env, map, atom_action, enif_make_binary(env, &action_bin), &map);

    udev_device_unref(dev);
    return map;
  } else {
    return atom_error;
  }
}

static ErlNifFunc nif_funcs[] = {
  {"start", 0, start},
  {"stop", 1, stop},
  {"poll", 1, poll},
  {"receive_device", 1, receive_device}
};

ERL_NIF_INIT(Elixir.Udev, nif_funcs, &load, &reload, &upgrade, &unload)
