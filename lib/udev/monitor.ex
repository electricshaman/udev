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
    send(state.listener, dev)
    Logger.debug "Event (#{time}µs): #{inspect dev}"
    Udev.poll(res)
    {:noreply, state}
  end
end
