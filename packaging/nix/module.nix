# NixOS module for OnAir. Untested on a real NixOS machine; see docs/NIXOS.md.
# The flake wraps this so that `package` and `serverPackage` default to the flake's own builds.
{ self }:
{ config, lib, pkgs, ... }:
let
  cfg = config.programs.onair;
  scfg = config.services.onair-server;
  system = pkgs.stdenv.hostPlatform.system;
in
{
  options.programs.onair = {
    enable = lib.mkEnableOption "OnAir, the software radio receiver (installs it and the udev rules for the USB radios)";
    package = lib.mkOption {
      type = lib.types.package;
      default = self.packages.${system}.onair;
      defaultText = lib.literalExpression "onair.packages.\${system}.onair";
      description = "The OnAir package. Use `.override { soapyModules = [ ... ]; extraRuntimeLibs = [ ... ]; }` for more radios.";
    };
    group = lib.mkOption {
      type = lib.types.str;
      default = "plugdev";
      description = "Group named by packaging/linux/60-onair-sdr.rules (MODE 0660). The rules also tag the radios with uaccess, so the user at the desk gets access without the group.";
    };
    users = lib.mkOption {
      type = lib.types.listOf lib.types.str;
      default = [ ];
      example = [ "alice" ];
      description = "Users added to the radio group (needed over ssh or without a local login session).";
    };
    blacklistTvDrivers = lib.mkOption {
      type = lib.types.bool;
      default = true;
      description = "Keep the kernel's DVB-T stick drivers off RTL-SDR dongles and its Mirics driver off SDRplay (the lists of packaging/linux/onair-*-blacklist.conf).";
    };
  };

  options.services.onair-server = {
    enable = lib.mkEnableOption "the OnAir headless server (a software radio controlled from a web browser, docs/headless-server.md)";
    package = lib.mkOption {
      type = lib.types.package;
      default = self.packages.${system}.onair-server;
      defaultText = lib.literalExpression "onair.packages.\${system}.onair-server";
      description = "The onair-server package.";
    };
    extraArgs = lib.mkOption {
      type = lib.types.listOf lib.types.str;
      default = [ ];
      example = [ "--lan" ];
      description = "Extra command line arguments. For access from other computers run `onair-server --lan --new-token` once as the onair user, which writes the token to the settings file.";
    };
    openFirewall = lib.mkOption {
      type = lib.types.bool;
      default = false;
      description = "Open the server's TCP port (8765 by default, see docs/headless-server.md) in the firewall. Unverified: set `port` to match your settings file.";
    };
    port = lib.mkOption {
      type = lib.types.port;
      default = 8765;
      description = "Port opened by openFirewall; it must match the port in the server settings file.";
    };
  };

  config = lib.mkMerge [
    (lib.mkIf (cfg.enable || scfg.enable) {
      services.udev.packages = [ (if cfg.enable then cfg.package else scfg.package) ];
      users.groups.${cfg.group} = { };
      users.users = lib.genAttrs cfg.users (_: { extraGroups = [ cfg.group ]; });
      boot.blacklistedKernelModules = lib.mkIf cfg.blacklistTvDrivers [
        "dvb_usb_rtl28xxu" "rtl2832" "rtl2830" "rtl2832_sdr" "e4000"
        "sdr_msi3101" "msi001" "msi2500"
      ];
    })

    (lib.mkIf cfg.enable {
      environment.systemPackages = [ cfg.package ];
    })

    (lib.mkIf scfg.enable {
      environment.systemPackages = [ scfg.package ];
      networking.firewall.allowedTCPPorts = lib.mkIf scfg.openFirewall [ scfg.port ];
      users.users.onair = {
        isSystemUser = true;
        group = "onair";
        extraGroups = [ cfg.group ];
        home = "/var/lib/onair";
      };
      users.groups.onair = { };
      # hardening copied from packaging/linux/onair-server.service
      systemd.services.onair-server = {
        description = "OnAir headless server (software radio controlled from a web browser)";
        wantedBy = [ "multi-user.target" ];
        after = [ "network-online.target" ];
        wants = [ "network-online.target" ];
        serviceConfig = {
          Type = "simple";
          User = "onair";
          Group = cfg.group;
          ExecStart = lib.escapeShellArgs ([ "${scfg.package}/bin/onair-server" "--config" "/var/lib/onair/server.conf" ] ++ scfg.extraArgs);
          Restart = "on-failure";
          RestartSec = 5;
          StateDirectory = "onair";
          StateDirectoryMode = "0750";
          NoNewPrivileges = true;
          ProtectSystem = "full";
          ProtectHome = true;
          PrivateTmp = true;
          ProtectKernelTunables = true;
          ProtectControlGroups = true;
          RestrictSUIDSGID = true;
          LockPersonality = true;
          CapabilityBoundingSet = "";
        };
      };
    })
  ];
}
