# Development shell: `nix develop`, then
#   cmake -S . -B build -G Ninja -DDECT2_BUILD_APP=ON && cmake --build build
{ pkgs, onair }:
pkgs.mkShell {
  inputsFrom = [ onair ]; # every build input of the package (compiler, cmake, pkg-config, cargo, rustc, hackrf, libusb, ffmpeg, glfw, soapysdr, hamlib)
  packages = with pkgs; [
    ninja
    gdb
    rtl-sdr
    airspy
    airspyhf
    bladeRF
    libiio
    alsa-lib
    libpulseaudio
    zenity
  ];
  # libraries OnAir opens with dlopen when it runs from the build folder
  LD_LIBRARY_PATH = pkgs.lib.makeLibraryPath (with pkgs; [
    hackrf libusb1 rtl-sdr airspy airspyhf bladeRF libiio soapysdr hamlib alsa-lib libpulseaudio libGL
  ]);
}
