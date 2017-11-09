defmodule Udev.Monitor do
  use GenServer
  require Logger

  def start_link do
    GenServer.start_link(__MODULE__, [])
  end

  def init(_args) do
    {:ok, res} = Udev.start()
    :ok = Udev.poll(res)
    {:ok, %{res: res}}
  end

  def handle_info({:select, res, _ref, :ready_input}, state) do
    dev = Udev.receive_device(res)
    Logger.debug "Event: #{inspect dev}"
    :ok = Udev.poll(res)
    {:noreply, state}
  end
end
