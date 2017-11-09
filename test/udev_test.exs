defmodule UdevTest do
  use ExUnit.Case
  doctest Udev

  test "greets the world" do
    assert Udev.hello() == :world
  end
end
