{
  description = "OnAir: software radio receiver (DVB-T2, DVB-T, ATSC, FM and many more modes) for HackRF and other SDRs";

  # Untested on a real Nix machine, written from CMakeLists.txt; docs/NIXOS.md lists what is unverified.
  inputs.nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";

  outputs = { self, nixpkgs }:
    let
      systems = [ "x86_64-linux" "aarch64-linux" ];
      forAll = f: nixpkgs.lib.genAttrs systems (system: f system nixpkgs.legacyPackages.${system});
      # SoapySDR modules put on the wrapper's plug-in path; names that this nixpkgs lacks are skipped
      soapyFor = pkgs: builtins.filter (p: p != null) (map (n: pkgs.${n} or null) [
        "soapyrtlsdr" "soapyhackrf" "soapyairspy" "soapyremote"
      ]);
    in
    {
      packages = forAll (system: pkgs: rec {
        onair = pkgs.callPackage ./packaging/nix/package.nix {
          src = self;
          soapyModules = soapyFor pkgs;
        };
        onair-server = pkgs.callPackage ./packaging/nix/package.nix {
          src = self;
          server = true;
          soapyModules = soapyFor pkgs;
        };
        default = onair;
      });

      apps = forAll (system: pkgs: rec {
        onair = { type = "app"; program = "${self.packages.${system}.onair}/bin/onair"; };
        onair-server = { type = "app"; program = "${self.packages.${system}.onair-server}/bin/onair-server"; };
        default = onair;
      });

      devShells = forAll (system: pkgs: {
        default = import ./packaging/nix/shell.nix { inherit pkgs; onair = self.packages.${system}.onair; };
      });

      nixosModules = rec {
        onair = import ./packaging/nix/module.nix { inherit self; };
        default = onair;
      };

      formatter = forAll (system: pkgs: pkgs.nixpkgs-fmt);
    };
}
