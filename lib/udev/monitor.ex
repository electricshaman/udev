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
    {:ok, res} = Udev.start(name)
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
    {time, raw_dev} = :timer.tc(fn -> Udev.receive_device(res) end)
    dev = struct(Udev.Device, raw_dev)
    event = {state.name, dev}
    send(state.listener, event)
    :ok = Udev.poll(res)

    Logger.debug "Event (#{time}µs): #{inspect event}"

    {:noreply, state}
  end
end
