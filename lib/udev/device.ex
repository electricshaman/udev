defmodule Udev.Device do
  @moduledoc """
  Kernel system device.
  """

  defstruct [
    :syspath,
    :subsystem,
    :sysname,
    :sysnum,
    :major,
    :minor,
    :devpath,
    :devtype,
    :devnode,
    :driver,
    :action,
    :seqnum,
    :sysattr,
    :tags
  ]

end
