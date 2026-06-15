self:
{ config, lib, pkgs, ... }:

let
  cfg = config.programs.blucast;
  pkg = self.packages.${pkgs.system}.default;
in
{
  options.programs.blucast = {
    enable = lib.mkEnableOption "BluCast virtual camera and microphone";
    package = lib.mkOption {
      type = lib.types.package;
      default = pkg;
      description = "The BluCast package to use.";
    };
  };

  config = lib.mkIf cfg.enable {
    environment.systemPackages = [ cfg.package ];

    # ── Virtual camera (v4l2loopback) ──────────────────────────────────────
    boot.extraModulePackages = [ config.boot.kernelPackages.v4l2loopback ];
    boot.kernelModules = [ "v4l2loopback" ];
    boot.extraModprobeConfig = ''
      options v4l2loopback devices=1 video_nr=10 card_label="BluCast Virtual Camera" exclusive_caps=1 max_buffers=2 max_openers=10
    '';
    services.udev.extraRules = ''
      SUBSYSTEM=="video4linux", ATTR{name}=="BluCast Virtual Camera", MODE="0660", TAG+="uaccess"
    '';

    # ── GPU container runtime ──────────────────────────────────────────────
    virtualisation.podman.enable = lib.mkDefault true;
    hardware.nvidia-container-toolkit.enable = lib.mkDefault true;

    # ── Virtual microphone (PipeWire null sink + remap source) ─────────────
    # If your nixpkgs predates services.pipewire.extraConfig, drop the same
    # load-module lines into environment.etc."pipewire/pipewire-pulse.conf.d/...".
    services.pipewire.extraConfig.pipewire-pulse."99-blucast" = {
      "pulse.cmd" = [
        {
          cmd = "load-module";
          args = "module-null-sink sink_name=BluCast_Mic_Sink rate=48000 sink_properties=device.description=BluCast_Mic_Sink";
        }
        {
          cmd = "load-module";
          args = "module-remap-source source_name=BluCast_Virtual_Microphone master=BluCast_Mic_Sink.monitor source_properties=device.description='BluCast Virtual Microphone'";
        }
      ];
    };
  };
}
