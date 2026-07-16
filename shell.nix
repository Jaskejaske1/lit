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
    if git_root="$(${pkgs.git}/bin/git rev-parse --show-toplevel 2>/dev/null)"; then
      export LIT_PROJECT_ROOT="$git_root"
    else
      export LIT_PROJECT_ROOT="$PWD"
    fi

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

    export LIT_BUILD_PROFILE="''${LIT_BUILD_PROFILE:-debug}"
    export LIT_BUILD_DIR="''${LIT_BUILD_DIR:-$LIT_PROJECT_ROOT/cmake-build/linux}"

    lit_configure() {
      local profile=''${1:-$LIT_BUILD_PROFILE}
      local build_type

      case "$profile" in
        debug) build_type=Debug ;;
        release) build_type=Release ;;
        *)
          echo "usage: lit_configure [debug|release]" >&2
          return 1
          ;;
      esac

      cmake -S "$LIT_PROJECT_ROOT" -B "$LIT_BUILD_DIR" -G Ninja -DCMAKE_BUILD_TYPE="$build_type"
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
    echo "  project   : $LIT_PROJECT_ROOT"
    echo "  build dir : $LIT_BUILD_DIR"
    echo "  profile   : $LIT_BUILD_PROFILE"
    echo "  commands  : lit_configure | lit_build | lit_test | lit_view"
  '';
}
