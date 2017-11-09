#include <libudev.h>
#include <stdio.h>
#include <stdlib.h>
#include <locale.h>
#include <unistd.h>
#include "erl_nif.h"

/*#define DEBUG*/

typedef struct {
  struct udev *udev;
} udev_priv;

int enumerate_subsystem(struct udev* udev, const char* subsystem)
{
	struct udev_enumerate *enumerate;
  int rc;

  udev = udev_new();
  if (!udev) {
    printf("Can't create udev\n");
    exit(1);
  }

  enumerate = udev_enumerate_new(udev);
  udev_enumerate_add_match_subsystem(enumerate, subsystem);
  rc = udev_enumerate_scan_devices(enumerate);

  struct udev_list_entry* devices = udev_enumerate_get_list_entry(enumerate);
  struct udev_list_entry* entry;

  udev_list_entry_foreach(entry, devices) {
    const char* path = udev_list_entry_get_name(entry);
    struct udev_device* dev = udev_device_new_from_syspath(udev, path);
    if (dev) {
      if (udev_device_get_devnode(dev))
      {
        const char* action = udev_device_get_action(dev);
        if (!action)
            action = "exists";

        const char* vendor = udev_device_get_sysattr_value(dev, "idVendor");
        if (!vendor)
            vendor = "0000";

        const char* product = udev_device_get_sysattr_value(dev, "idProduct");
        if (!product)
            product = "0000";

        printf("%s %s %6s %s:%s %s\r\n",
               udev_device_get_subsystem(dev),
               udev_device_get_devtype(dev),
               action,
               vendor,
               product,
               udev_device_get_devnode(dev));
      }

      udev_device_unref(dev);
    }
  }

	udev_enumerate_unref(enumerate);

  return rc;
}

static ERL_NIF_TERM
enumerate(ErlNifEnv *env, int argc, const ERL_NIF_TERM argv[])
{
  udev_priv* priv;
  ErlNifBinary subsystem;
  int subcount;

  if(!enif_inspect_binary(env, argv[0], &subsystem))
    return enif_make_badarg(env);

  priv = enif_priv_data(env);

  subcount = enumerate_subsystem(priv->udev, (const char *)subsystem.data);

  #ifdef DEBUG
  unsigned i;
  printf("received binary of length %zu\r\ndata: ", subsystem.size);
  for (i = 0; i < subsystem.size; ++i)
      printf("%c", subsystem.data[i]);
  printf("\r\n");
  #endif

  return enif_make_int(env, subcount);
}

static int
load(ErlNifEnv* env, void** priv, ERL_NIF_TERM info) {
  udev_priv* data = enif_alloc(sizeof(udev_priv));
  if (data == NULL) {
    return 1;
  }

  struct udev *udev;

  udev = udev_new();
  if (!udev) {
    printf("Can't create udev\n");
    exit(1);
  }

  data->udev = udev;

  *priv = (void*) data;
  return 0;
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
  udev_priv* upriv = (udev_priv*) priv;

	udev_unref(upriv->udev);
  enif_free(priv);
}

static ErlNifFunc nif_funcs[] = {
  {"enumerate", 1, enumerate, ERL_NIF_DIRTY_JOB_IO_BOUND},
};

ERL_NIF_INIT(Elixir.Udev, nif_funcs, &load, &reload, &upgrade, &unload)
