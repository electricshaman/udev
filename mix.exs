defmodule Udev.Mixfile do
  use Mix.Project

  def project do
    [
      app: :udev,
      version: "0.1.0",
      elixir: "~> 1.5",
      start_permanent: Mix.env == :prod,
      deps: deps(),
      compilers: [:elixir_make] ++ Mix.compilers,
      make_clean: ["clean"],
      description: description(),
      package: package(),
      name: "Udev",
      source_url: "https://github.com/electricshaman/udev"
    ]
  end

  defp description do
    "Experimental libudev NIF for Linux"
  end

  def application do
    [
      extra_applications: [:logger],
      mod: {Udev.Application, []}
    ]
  end

  defp deps do
    [
      {:elixir_make, "~> 0.4", runtime: false}
    ]
  end

  defp package() do
    [
      files: ["lib", "src/*.[ch]", "Makefile", "LICENSE", "mix.exs", "README.md"],
      maintainers: ["Jeff Smith"],
      licenses: ["Apache 2.0"],
      links: %{"GitHub" => "https://github.com/electricshaman/udev"}
    ]
  end
end
