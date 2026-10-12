{ pkgs ? import <nixpkgs> {} }:
let
  libs = with pkgs; [
    stdenv.cc.cc.lib glib nss nspr dbus at-spi2-atk at-spi2-core atk libdrm
    xorg.libX11 xorg.libXcomposite xorg.libXdamage xorg.libXext xorg.libXfixes
    xorg.libXrandr xorg.libxcb xorg.libXrender xorg.libXi xorg.libXtst xorg.libXcursor
    xorg.libXScrnSaver libxkbcommon mesa libgbm libGL expat pango cairo alsa-lib
    fontconfig freetype zlib libpng cups systemd
  ];
in pkgs.mkShell {
  buildInputs = libs ++ (with pkgs; [ cmake ninja jq unzip ]);
  NIX_LD_LIBRARY_PATH = pkgs.lib.makeLibraryPath libs;
  NIX_LD = pkgs.lib.fileContents "${pkgs.stdenv.cc}/nix-support/dynamic-linker";
}
