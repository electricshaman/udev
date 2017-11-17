defmodule Udev.MonitorTest do
  use ExUnit.Case
  alias Udev.Monitor

  test "return error if name other than udev or kernel is passed" do
    assert {:error, _} = Monitor.start_link(:stuff)
    assert {:ok, pid1} = Monitor.start_link(:udev)
    assert {:ok, pid2} = Monitor.start_link(:kernel)
    Monitor.stop(pid1)
    Monitor.stop(pid2)
  end
end
