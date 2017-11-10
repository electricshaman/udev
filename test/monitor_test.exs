defmodule Udev.MonitorTest do
  use ExUnit.Case
  alias Udev.Monitor

  test "decorate device with empty vid/pid" do
    assert %{vid_pid: nil} = Monitor.decorate_device(%{vid: nil, pid: nil})
  end

  test "decorate device with vid_pid" do
    assert %{vid_pid: "0000:0000"} = Monitor.decorate_device(%{vid: "0000", pid: "0000"})
  end

  test "decorate device with empty major/minor" do
    assert %{major_minor: nil} = Monitor.decorate_device(%{major: nil, minor: nil})
  end

  test "decorate device with major/minor" do
    assert %{major_minor: {42, 0}} = Monitor.decorate_device(%{major: 42, minor: 0})
  end
end
