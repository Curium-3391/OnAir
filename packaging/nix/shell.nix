# Development shell: `nix develop`, then
#   cmake -S . -B build -G Ninja -DDECT2_BUILD_APP=ON && cmake --build build
{ pkgs, onair }:
let
  inherit (pkgs) lib;
in
pkgs.mkShell {
  inputsFrom = [ onair ]; # every build input of the package (compiler, cmake, pkg-config, cargo, rustc, hackrf, libusb, ffmpeg, glfw, soapysdr, hamlib)
  packages =
    (with pkgs; [
      ninja
      gdb
      git
      rtl-sdr
      alsa-lib
      libpulseaudio
      zenity
    ])
    ++ lib.optional (pkgs ? airspy) pkgs.airspy
    ++ lib.optional (pkgs ? airspyhf) pkgs.airspyhf
    ++ lib.optional (pkgs ? libbladeRF) pkgs.libbladeRF
    ++ lib.optional (pkgs ? libiio) pkgs.libiio;
  # libraries OnAir opens with dlopen when it runs from the build folder
  LD_LIBRARY_PATH = lib.makeLibraryPath (
    (with pkgs; [ hackrf libusb1 rtl-sdr soapysdr hamlib alsa-lib libpulseaudio libGL ])
    ++ lib.optional (pkgs ? airspy) pkgs.airspy
    ++ lib.optional (pkgs ? airspyhf) pkgs.airspyhf
    ++ lib.optional (pkgs ? libbladeRF) pkgs.libbladeRF
    ++ lib.optional (pkgs ? libiio) pkgs.libiio
  );
}
