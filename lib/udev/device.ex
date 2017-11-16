defmodule Udev.Device do
  @moduledoc """
  Kernel system device.
  """

  require Logger

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
    :seqnum
  ]

  def new_from_syspath(syspath) do
    find_device(fn ->
      Udev.new_from_syspath(to_charlist(syspath))
    end)
  end

  def get_parent_with_subsystem_devtype(%__MODULE__{} = device, subsystem, devtype) do
    find_device(fn ->
      Udev.get_parent_with_subsystem_devtype(
        to_charlist(device.syspath), to_charlist(subsystem), to_charlist(devtype))
    end)
  end

  defp find_device(fun) do
    case :timer.tc(fun) do
      {time, {:ok, device}} ->
        Logger.debug "Found device (#{time}µs): #{inspect device}"
        {:ok, struct(Udev.Device, device)}
      {time, {:error, _} = error} ->
        Logger.error "Device not found (#{time}µs): #{inspect error}"
        error
    end
  end
end
