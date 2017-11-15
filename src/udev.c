#include <libudev.h>
#include <stdio.h>
#include <stdlib.h>
#include <locale.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <linux/kdev_t.h>
#include "erl_nif.h"

/*#define DEBUG*/

#define ASSERT(e) ((void)((e) ? 1 : (assert_error(#e, __func__, __FILE__, __LINE__), 0)))
#define SET_NONBLOCKING(fd) fcntl((fd), F_SETFL, fcntl((fd), F_GETFL, 0) | O_NONBLOCK)

static void assert_error(const char *expr, const char *func, const char *file, int line)
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
  int open;
} Monitor;

typedef struct {
  ERL_NIF_TERM atom_ok;
  ERL_NIF_TERM atom_undefined;
  ERL_NIF_TERM atom_error;
  ERL_NIF_TERM atom_nil;
  ERL_NIF_TERM atom_add;
  ERL_NIF_TERM atom_remove;
  ERL_NIF_TERM atom_change;
  ERL_NIF_TERM atom_online;
  ERL_NIF_TERM atom_offline;
  ERL_NIF_TERM atom_devnode;
  ERL_NIF_TERM atom_subsystem;
  ERL_NIF_TERM atom_devtype;
  ERL_NIF_TERM atom_major;
  ERL_NIF_TERM atom_minor;
  ERL_NIF_TERM atom_action;
  ERL_NIF_TERM atom_devpath;
  ERL_NIF_TERM atom_syspath;
  ERL_NIF_TERM atom_sysname;
  ERL_NIF_TERM atom_sysnum;
  ERL_NIF_TERM atom_vid;
  ERL_NIF_TERM atom_pid;
  ERL_NIF_TERM atom_man;
  ERL_NIF_TERM atom_prod;
  ERL_NIF_TERM atom_serial;
  ERL_NIF_TERM atom_driver;
  ERL_NIF_TERM atom_seqnum;
} monitor_priv;

static void mon_rt_dtor(ErlNifEnv *env, void *obj)
{
  enif_fprintf(stderr, "mon_rt_dtor called\n");
}

static void mon_rt_stop(ErlNifEnv *env, void *obj, int fd, int is_direct_call)
{
  Monitor *rt = (Monitor *)obj;
  enif_fprintf(stderr, "mon_rt_stop called %s\n", (is_direct_call ? "DIRECT" : "LATER"));
  udev_monitor_unref(rt->monitor);
  udev_unref(rt->context);
}

static void mon_rt_down(ErlNifEnv* env, void* obj, ErlNifPid* pid, ErlNifMonitor* mon)
{
  Monitor *rt = (Monitor *)obj;
  int rv;

  enif_fprintf(stderr, "mon_rt_down called\n");

  if(rt->open) {
    ERL_NIF_TERM undefined;
    enif_make_existing_atom(env, "undefined", &undefined, ERL_NIF_LATIN1);
    rv = enif_select(env, rt->fd, ERL_NIF_SELECT_STOP, rt, NULL, undefined);
    ASSERT(rv >= 0);
  }
}

static ErlNifResourceType *mon_rt;
static ErlNifResourceTypeInit mon_rt_init = {mon_rt_dtor, mon_rt_stop, mon_rt_down};

static int load(ErlNifEnv* env, void** priv, ERL_NIF_TERM info) {
  monitor_priv* data = enif_alloc(sizeof(monitor_priv));
  if (data == NULL) {
    return 1;
  }

  data->atom_ok = enif_make_atom(env, "ok");
  data->atom_undefined = enif_make_atom(env, "undefined");
  data->atom_error = enif_make_atom(env, "error");
  data->atom_nil = enif_make_atom(env, "nil");
  data->atom_add = enif_make_atom(env, "add");
  data->atom_remove = enif_make_atom(env, "remove");
  data->atom_change = enif_make_atom(env, "change");
  data->atom_online = enif_make_atom(env, "online");
  data->atom_offline = enif_make_atom(env, "offline");
  data->atom_devnode = enif_make_atom(env, "devnode");
  data->atom_subsystem = enif_make_atom(env, "subsystem");
  data->atom_devtype = enif_make_atom(env, "devtype");
  data->atom_major = enif_make_atom(env, "major");
  data->atom_minor = enif_make_atom(env, "minor");
  data->atom_action = enif_make_atom(env, "action");
  data->atom_devpath = enif_make_atom(env, "devpath");
  data->atom_syspath = enif_make_atom(env, "syspath");
  data->atom_sysname = enif_make_atom(env, "sysname");
  data->atom_sysnum = enif_make_atom(env, "sysnum");
  data->atom_driver = enif_make_atom(env, "driver");
  data->atom_seqnum = enif_make_atom(env, "seqnum");

  *priv = (void*) data;

  mon_rt = enif_open_resource_type_x(env, "monitor", &mon_rt_init, ERL_NIF_RT_CREATE, NULL);

  return !mon_rt;
}

static int reload(ErlNifEnv* env, void** priv, ERL_NIF_TERM info) {
  return 0;
}

static int upgrade(ErlNifEnv* env, void** priv, void** old_priv, ERL_NIF_TERM info) {
  return load(env, priv, info);
}

static void unload(ErlNifEnv* env, void* priv) {
  enif_free(priv);
}

static ERL_NIF_TERM mon_start(ErlNifEnv *env, int argc, const ERL_NIF_TERM argv[])
{
  Monitor *mon;
  ERL_NIF_TERM res;
  int fd;
  struct udev *context;
  struct udev_monitor *udev_mon;
  monitor_priv* priv = enif_priv_data(env);
  ErlNifBinary name_bin;
  ErlNifPid self;

  if (enif_inspect_binary(env, argv[0], &name_bin) == 0) {
    return enif_make_badarg(env);
  }

  char name[name_bin.size + 1];
  *name = '\0';
  strncat(name, (char*) name_bin.data, name_bin.size);
  enif_release_binary(&name_bin);

  if(strcmp(name, "udev") > 0 && strcmp(name, "kernel") > 0) {
    return enif_make_badarg(env);
  }

  context = udev_new();
  if(!context) {
    enif_fprintf(stderr, "Can't create udev context\n");
    return enif_make_badarg(env);
  }

  enif_self(env, &self);

  udev_mon = udev_monitor_new_from_netlink(context, name);
  udev_monitor_enable_receiving(udev_mon);
  fd = udev_monitor_get_fd(udev_mon);

  SET_NONBLOCKING(fd);

  mon = enif_alloc_resource(mon_rt, sizeof(Monitor));
  mon->context = context;
  mon->monitor = udev_mon;
  mon->fd = fd;
  mon->open = 1;

  enif_monitor_process(env, mon, &self, NULL);

  res = enif_make_resource(env, mon);
  enif_release_resource(mon);

  return enif_make_tuple2(env, priv->atom_ok, res);
}

static ERL_NIF_TERM mon_stop(ErlNifEnv *env, int argc, const ERL_NIF_TERM argv[])
{
  Monitor *mon;
  monitor_priv* priv = enif_priv_data(env);
  int do_stop;

  if(!enif_get_resource(env, argv[0], mon_rt, (void **)&mon)) {
    return enif_make_badarg(env);
  }

  do_stop = mon->open;
  mon->open = 0;

  if(do_stop) {
    enif_fprintf(stderr, "Closing fd=%d\n", mon->fd);
    int rv = enif_select(env, mon->fd, ERL_NIF_SELECT_STOP, mon, NULL, priv->atom_undefined);
    ASSERT(rv >= 0);
  }

  return priv->atom_ok;
}

static ERL_NIF_TERM mon_poll(ErlNifEnv *env, int argc, const ERL_NIF_TERM argv[])
{
  Monitor *mon;
  int rv;
  monitor_priv* priv = enif_priv_data(env);

  if(!enif_get_resource(env, argv[0], mon_rt, (void **)&mon)) {
    return enif_make_badarg(env);
  }

  /*enif_fprintf(stderr, "Polling fd=%d\n", mon->fd);*/

  rv = enif_select(env, mon->fd, ERL_NIF_SELECT_READ, mon, NULL, priv->atom_undefined);
  ASSERT(rv >= 0);

  return priv->atom_ok;
}

static int map_put_string(ErlNifEnv *env, ERL_NIF_TERM map_in, ERL_NIF_TERM* map_out, ERL_NIF_TERM key, const char *value)
{
  ErlNifBinary bin;
  monitor_priv* priv = enif_priv_data(env);

  if(value == NULL) {
    return enif_make_map_put(env, map_in, key, priv->atom_nil, map_out);
  }

  enif_alloc_binary(strlen(value), &bin);
  memcpy(bin.data, value, strlen(value));

  return enif_make_map_put(env, map_in, key, enif_make_binary(env, &bin), map_out);
}

static int map_put(ErlNifEnv *env, ERL_NIF_TERM map_in, ERL_NIF_TERM* map_out, ERL_NIF_TERM key, ERL_NIF_TERM value)
{
  return enif_make_map_put(env, map_in, key, value, map_out);
}

static ERL_NIF_TERM mon_receive_device(ErlNifEnv *env, int argc, const ERL_NIF_TERM argv[])
{
  Monitor *mon;
  struct udev_device *dev;
  monitor_priv* priv = enif_priv_data(env);

  if(!enif_get_resource(env, argv[0], mon_rt, (void **)&mon)) {
    return enif_make_badarg(env);
  }

  /*enif_fprintf(stderr, "Reading fd=%d\n", mon->fd);*/

  dev = udev_monitor_receive_device(mon->monitor);

  if(dev) {
    ERL_NIF_TERM map = enif_make_new_map(env);
    dev_t devnum = udev_device_get_devnum(dev);

    ERL_NIF_TERM atom_action;
    const char *action = udev_device_get_action(dev);
    enif_make_existing_atom(env, action, &atom_action, ERL_NIF_LATIN1);

    map_put(env, map, &map, priv->atom_action, atom_action);
    map_put(env, map, &map, priv->atom_seqnum, enif_make_long(env, udev_device_get_seqnum(dev)));

    map_put_string(env, map, &map, priv->atom_devnode, udev_device_get_devnode(dev));
    map_put_string(env, map, &map, priv->atom_subsystem, udev_device_get_subsystem(dev));
    map_put_string(env, map, &map, priv->atom_devtype, udev_device_get_devtype(dev));
    map_put_string(env, map, &map, priv->atom_devpath, udev_device_get_devpath(dev));
    map_put_string(env, map, &map, priv->atom_syspath, udev_device_get_syspath(dev));
    map_put_string(env, map, &map, priv->atom_sysname, udev_device_get_sysname(dev));
    map_put_string(env, map, &map, priv->atom_sysnum, udev_device_get_sysnum(dev));
    map_put_string(env, map, &map, priv->atom_driver, udev_device_get_driver(dev));
    map_put(env, map, &map, priv->atom_major, enif_make_uint(env, MAJOR(devnum)));
    map_put(env, map, &map, priv->atom_minor, enif_make_uint(env, MINOR(devnum)));

    udev_device_unref(dev);

    return map;
  } else {
    return priv->atom_error;
  }
}

static ErlNifFunc nif_funcs[] = {
  {"start", 1, mon_start},
  {"stop", 1, mon_stop},
  {"poll", 1, mon_poll},
  {"receive_device", 1, mon_receive_device}
};

ERL_NIF_INIT(Elixir.Udev, nif_funcs, &load, &reload, &upgrade, &unload)
