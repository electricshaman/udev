defmodule Udev.Monitor do
  use GenServer
  require Logger

  def start_link do
    start_link(self())
  end

  def start_link(listener) do
    GenServer.start_link(__MODULE__, [listener])
  end

  def init([listener]) do
    {:ok, res} = Udev.start()
    :ok = Udev.poll(res)
    {:ok, %{res: res, listener: listener}}
  end

  def stop(pid) do
    GenServer.call(pid, :stop)
  end

  def handle_call(:stop, _from, %{res: res} = state) do
    :ok = Udev.stop(res)
    {:reply, :ok, state}
  end

  def handle_info({:select, res, _ref, :ready_input}, state) do
    {time, dev} = :timer.tc(fn -> Udev.receive_device(res) end)
    dev = decorate_device(dev)

    send(state.listener, dev)
    Logger.debug "Event (#{time}µs): #{inspect dev}"
    :ok = Udev.poll(res)

    {:noreply, state}
  end

  def decorate_device(dev) do
    decorate_device(dev, [:vid_pid, :major_minor])
  end

  defp decorate_device(%{vid: nil, pid: nil} = dev, [:vid_pid|t]) do
    Map.put(dev, :vid_pid, nil)
    |> decorate_device(t)
  end
  defp decorate_device(%{vid: vid, pid: pid} = dev, [:vid_pid|t]) do
    Map.put(dev, :vid_pid, "#{vid}:#{pid}")
    |> decorate_device(t)
  end

  defp decorate_device(%{major: nil, minor: nil} = dev, [:major_minor|t]) do
    Map.put(dev, :major_minor, nil)
    |> decorate_device(t)
  end
  defp decorate_device(%{major: major, minor: minor} = dev, [:major_minor|t]) do
    Map.put(dev, :major_minor, {major, minor})
    |> decorate_device(t)
  end

  defp decorate_device(dev, [_h|t]) do
    decorate_device(dev, t)
  end

  defp decorate_device(dev, []) do
    dev
  end
end
