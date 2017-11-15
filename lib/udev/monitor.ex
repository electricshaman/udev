defmodule Udev.Monitor do
  use GenServer
  require Logger

  @names [:udev, :kernel]

  @doc """
  Start a new udev or kernel (uevent) monitor process.
  """
  def start_link(name \\ :udev)

  def start_link(name) when name in @names do
    start_link(self(), name)
  end

  def start_link(_name), do: {:error, :badarg}

  def start_link(listener, name) when name in @names do
    GenServer.start_link(__MODULE__, [listener, name])
  end

  def start_link(_listener, _name), do: {:error, :badarg}

  def init([listener, name]) do
    {:ok, res} = Udev.start(to_string(name))
    :ok = Udev.poll(res)
    {:ok, %{res: res, listener: listener, name: name}}
  end

  @doc """
  Stop a running udev or kernel (uevent) monitor.
  """
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
    event = {state.name, dev}
    send(state.listener, event)
    Logger.debug "Event (#{time}µs): #{inspect event}"
    :ok = Udev.poll(res)

    {:noreply, state}
  end

  def decorate_device(dev) do
    decorate_device(dev, [:devnum])
  end

  defp decorate_device(%{major: nil, minor: nil} = dev, [:devnum|t]) do
    Map.put(dev, :devnum, nil)
    |> decorate_device(t)
  end
  defp decorate_device(%{major: major, minor: minor} = dev, [:devnum|t]) do
    Map.put(dev, :devnum, {major, minor})
    |> decorate_device(t)
  end

  defp decorate_device(dev, [_h|t]) do
    decorate_device(dev, t)
  end

  defp decorate_device(dev, []) do
    dev
  end
end
