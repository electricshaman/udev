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
  struct udev *context;
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

  udev_unref(mon->context);
}

static ErlNifResourceType *mon_rt;
static ERL_NIF_TERM atom_ok;
static ERL_NIF_TERM atom_undefined;
static ERL_NIF_TERM atom_error;
static ERL_NIF_TERM atom_node;
static ERL_NIF_TERM atom_subsystem;
static ERL_NIF_TERM atom_devtype;
static ERL_NIF_TERM atom_action;
static ERL_NIF_TERM atom_devpath;
static ERL_NIF_TERM atom_syspath;
static ERL_NIF_TERM atom_sysname;
static ERL_NIF_TERM atom_sysnum;
static ERL_NIF_TERM atom_vid;
static ERL_NIF_TERM atom_pid;
static ERL_NIF_TERM atom_man;
static ERL_NIF_TERM atom_prod;
static ERL_NIF_TERM atom_serial;

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
  atom_devpath = enif_make_atom(env, "devpath");
  atom_syspath = enif_make_atom(env, "syspath");
  atom_sysname = enif_make_atom(env, "sysname");
  atom_sysnum = enif_make_atom(env, "sysnum");
  atom_vid = enif_make_atom(env, "vid");
  atom_pid = enif_make_atom(env, "pid");
  atom_man = enif_make_atom(env, "manufacturer");
  atom_prod = enif_make_atom(env, "product");
  atom_serial = enif_make_atom(env, "serial");

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
	struct udev *context;
  struct udev_monitor *udev_mon;

  context = udev_new();
  if(!context) {
    enif_fprintf(stderr, "Can't make udev context\n");
    return enif_make_badarg(env);
  }

  udev_mon = udev_monitor_new_from_netlink(context, "udev");
	udev_monitor_enable_receiving(udev_mon);
	fd = udev_monitor_get_fd(udev_mon);

  SET_NONBLOCKING(fd);

  mon = enif_alloc_resource(mon_rt, sizeof(Monitor));
  mon->context = context;
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

  /*enif_fprintf(stderr, "Polling fd=%d\n", mon->fd);*/

  rv = enif_select(env, mon->fd, ERL_NIF_SELECT_READ, mon, NULL, atom_undefined);
  ASSERT(rv >= 0);

  return atom_ok;
}

static int
map_put_string(ErlNifEnv *env, ERL_NIF_TERM map_in, ERL_NIF_TERM* map_out, ERL_NIF_TERM key, const char *value)
{
  ErlNifBinary val_bin;
  const char* val = value == NULL ? "" : value;

  enif_alloc_binary(strlen(val), &val_bin);
  memcpy(val_bin.data, val, strlen(val));

  return enif_make_map_put(env, map_in, key, enif_make_binary(env, &val_bin), map_out);
}

static ERL_NIF_TERM
receive_device(ErlNifEnv *env, int argc, const ERL_NIF_TERM argv[])
{
  Monitor *mon;
  struct udev_device *dev;

  if(!enif_get_resource(env, argv[0], mon_rt, (void **)&mon)) {
    return enif_make_badarg(env);
  }

  /*enif_fprintf(stderr, "Reading fd=%d\n", mon->fd);*/

  dev = udev_monitor_receive_device(mon->monitor);

  if(dev) {
    ERL_NIF_TERM map = enif_make_new_map(env);

    map_put_string(env, map, &map, atom_node, udev_device_get_devnode(dev));
    map_put_string(env, map, &map, atom_subsystem, udev_device_get_subsystem(dev));
    map_put_string(env, map, &map, atom_devtype, udev_device_get_devtype(dev));
    map_put_string(env, map, &map, atom_action, udev_device_get_action(dev));
    map_put_string(env, map, &map, atom_devpath, udev_device_get_devpath(dev));
    map_put_string(env, map, &map, atom_syspath, udev_device_get_syspath(dev));
    map_put_string(env, map, &map, atom_sysname, udev_device_get_sysname(dev));
    map_put_string(env, map, &map, atom_sysnum, udev_device_get_sysnum(dev));

    dev = udev_device_get_parent_with_subsystem_devtype(dev, "usb", "usb_device");
    if(dev) {
      map_put_string(env, map, &map, atom_vid, udev_device_get_sysattr_value(dev,"idVendor"));
      map_put_string(env, map, &map, atom_pid, udev_device_get_sysattr_value(dev,"idProduct"));
      map_put_string(env, map, &map, atom_man, udev_device_get_sysattr_value(dev,"manufacturer"));
      map_put_string(env, map, &map, atom_prod, udev_device_get_sysattr_value(dev,"product"));
      map_put_string(env, map, &map, atom_serial, udev_device_get_sysattr_value(dev,"serial"));
    } else {
      map_put_string(env, map, &map, atom_vid, NULL);
      map_put_string(env, map, &map, atom_pid, NULL);
      map_put_string(env, map, &map, atom_man, NULL);
      map_put_string(env, map, &map, atom_prod, NULL);
      map_put_string(env, map, &map, atom_serial, NULL);
    }

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