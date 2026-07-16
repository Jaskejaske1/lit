{ pkgs ? import <nixpkgs> {} }:

pkgs.mkShell {
  # Automatically pulls in every single library, header, and pkg-config file
  # needed to compile SDL3, OpenGL, and standard C++ graphics apps on Linux.
  inputsFrom = [ pkgs.sdl3 ];

  nativeBuildInputs = with pkgs; [
    cmake
    gdb
    ninja
    gcc13
    pkg-config
    python3
  ];

  buildInputs = with pkgs; [
    libGL
    mesa
    libxcb
    xcbutil
  ];

  shellHook = ''
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

    export LIT_BUILD_DIR="''${LIT_BUILD_DIR:-$PWD/cmake-build/linux}"

    lit_configure() {
      cmake -S "$PWD" -B "$LIT_BUILD_DIR" -G Ninja -DCMAKE_BUILD_TYPE=''${1:-Debug}
    }

    lit_build() {
      cmake --build "$LIT_BUILD_DIR"
    }

    lit_test() {
      "$LIT_BUILD_DIR/bin/test_substrate"
    }

    lit_view() {
      "$LIT_BUILD_DIR/bin/lit_view"
    }

    echo "⚡ lit NixOS dev shell active"
    echo "  build dir : $LIT_BUILD_DIR"
    echo "  commands  : lit_configure | lit_build | lit_test | lit_view"
  '';
}
