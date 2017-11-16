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

  def get_parent_with_subsystem_devtype(%__MODULE__{} = device, subsystem, devtype) do
    get_parent = fn ->
      Udev.get_parent_with_subsystem_devtype(
        to_charlist(device.syspath), to_charlist(subsystem), to_charlist(devtype))
    end

    case :timer.tc(get_parent) do
      {time, {:ok, parent}} ->
        Logger.debug "Found device parent in #{time}µs: #{inspect parent}"
        {:ok, struct(Udev.Device, parent)}
      {time, {:error, _} = error} ->
        Logger.error "Parent not found in #{time}µs: #{inspect error}"
        error
    end
  end
end
