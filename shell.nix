{ pkgs ? import <nixpkgs> {} }:

pkgs.mkShell {
  # Automatically pulls in every single library, header, and pkg-config file 
  # needed to compile SDL3, OpenGL, and standard C++ graphics apps on Linux.
  inputsFrom = [ pkgs.sdl3 ];

  nativeBuildInputs = with pkgs; [
    cmake
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
    echo "⚡ lit graphics dev shell active!"
  '';
}
