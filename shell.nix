{
  pkgs ? import <nixpkgs> { },
}:

let
  # Create a wrapper around clangd that tells it where GCC's standard library is
  clangdWrapper = pkgs.writeShellScriptBin "clangd" ''
    export CPLUS_INCLUDE_PATH="${pkgs.gcc13.cc}/include/c++/${pkgs.gcc13.cc.version}:${pkgs.gcc13.cc}/include/c++/${pkgs.gcc13.cc.version}/x86_64-unknown-linux-gnu''${CPLUS_INCLUDE_PATH:+:$CPLUS_INCLUDE_PATH}"
    exec ${pkgs.clang-tools}/bin/clangd "$@"
  '';

  litConfigure = pkgs.writeShellScriptBin "lit_configure" ''
    set -euo pipefail

    project_root="''${LIT_PROJECT_ROOT:-$PWD}"
    build_dir="''${LIT_BUILD_DIR:-$project_root/cmake-build/linux}"
    profile="''${1:-''${LIT_BUILD_PROFILE:-debug}}"

    case "$profile" in
      debug) build_type=Debug ;;
      release) build_type=Release ;;
      *)
        echo "usage: lit_configure [debug|release]" >&2
        exit 1
        ;;
    esac

    echo "==> Configuring lit ($build_type) in $build_dir"
    ${pkgs.cmake}/bin/cmake -S "$project_root" -B "$build_dir" -G Ninja \
      -DCMAKE_BUILD_TYPE="$build_type" \
      -DCMAKE_EXPORT_COMPILE_COMMANDS=ON

    ln -sf "$build_dir/compile_commands.json" "$project_root/compile_commands.json"
  '';

  litBuild = pkgs.writeShellScriptBin "lit_build" ''
    set -euo pipefail
    build_dir="''${LIT_BUILD_DIR:-$PWD/cmake-build/linux}"

    echo "==> Building lit in $build_dir"
    ${pkgs.cmake}/bin/cmake --build "$build_dir"
  '';

  litTest = pkgs.writeShellScriptBin "lit_test" ''
    set -euo pipefail
    build_dir="''${LIT_BUILD_DIR:-$PWD/cmake-build/linux}"

    echo "==> Running lit tests"
    "$build_dir/bin/test_substrate"
  '';
in
pkgs.mkShell {
  inputsFrom = [ pkgs.sdl3 ];

  nativeBuildInputs = with pkgs; [
    cmake
    gdb
    ninja
    gcc13
    pkg-config
    python3

    clangdWrapper # Use our wrapper instead of raw clang-tools

    litConfigure
    litBuild
    litTest
  ];

  buildInputs = with pkgs; [
    libGL
    mesa
    libxcb
    xcbutil
  ];

  shellHook = ''
    if git_root="$(${pkgs.git}/bin/git rev-parse --show-toplevel 2>/dev/null)"; then
      export LIT_PROJECT_ROOT="$git_root"
    else
      export LIT_PROJECT_ROOT="$PWD"
    fi

    export LIT_BUILD_PROFILE="debug"
    export LIT_BUILD_DIR="$LIT_PROJECT_ROOT/cmake-build/linux"

    nix_runtime_libs="$(
      printf '%s\n' "$NIX_LDFLAGS" \
        | tr ' ' '\n' \
        | sed -n 's/^-L//p' \
        | awk '!seen[$0]++' \
        | paste -sd :
    )"

    graphics_libs="/run/opengl-driver/lib:/run/opengl-driver-32/lib"

    if [ -n "$nix_runtime_libs" ]; then
      export LD_LIBRARY_PATH="$nix_runtime_libs:$graphics_libs''${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
    else
      export LD_LIBRARY_PATH="$graphics_libs''${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
    fi

    echo "⚡ lit NixOS dev environment loaded via direnv"
    echo "  project   : $LIT_PROJECT_ROOT"
    echo "  build dir : $LIT_BUILD_DIR"
    echo "  commands  : lit_configure | lit_build | lit_test"
  '';
}
