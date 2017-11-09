defmodule Udev do
  require Logger

  @on_load :load_nif
  @app     Mix.Project.config[:app]
  @compile {:autoload, false}

  def load_nif do
    so_path = Path.join(:code.priv_dir(@app), "udev")
    case :erlang.load_nif(so_path, 0) do
      :ok -> :ok
      {:error, {_reason, msg}} ->
        Logger.warn("Unable to load udev NIF: #{to_string(msg)}")
    end
  end

  def enumerate(_subsystem) do
    raise "NIF enumerate/1 not implemented"
  end
end
