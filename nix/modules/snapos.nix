{ config, lib, pkgs, ... }:

# The full SnapOS: every pack turned on, plus what makes a system SnapOS
# (its name and version, network, sound, graphics and cleaning).
# The packs alone (nix/packs) can be added to any NixOS system.
with lib;
let
  # The SnapOS release this tree is (the VERSION file at the repository root).
  snaposVersion = lib.strings.trim (builtins.readFile ../../VERSION);
in {
  imports = [ ../packs ];

  config = {
    snapos.packs = mapAttrs (_: _: { enable = mkDefault true; }) {
      theme = 0; desktops = 0; commands = 0; snapguard = 0; snapweb = 0; debian = 0; helper = 0; updater = 0; store = 0;
    };

    system.nixos.distroName = "SnapOS";
    networking.hostName = mkDefault "snapos";
    # Wi-Fi and Ethernet on every desktop (only Budgie's module turns this on
    # by itself; the installer keeps the Wi-Fi networks in NetworkManager)
    networking.networkmanager.enable = true;

    # os-release names the SnapOS release, not the NixOS one: fastfetch and
    # `snapos version` show "SnapOS 2.0", and updates compare this number.
    environment.etc."os-release".text = mkForce ''
      NAME="SnapOS"
      ID=snapos
      ID_LIKE=nixos
      VERSION="${snaposVersion}"
      VERSION_ID="${snaposVersion}"
      PRETTY_NAME="SnapOS ${snaposVersion}${optionalString pkgs.stdenv.hostPlatform.isAarch64 " ARM"}"${optionalString pkgs.stdenv.hostPlatform.isAarch64 "\nVARIANT=\"ARM64\"\nVARIANT_ID=arm64"}
      BUILD_ID="${config.system.nixos.version}"
      HOME_URL="https://github.com/snapos-distro/SnapOS"
      SUPPORT_URL="https://github.com/snapos-distro/SnapOS/issues"
      LOGO=snapos
    '';
    environment.etc."snapos/version".text = snaposVersion;

    # Budgie turns on Rygel, a DLNA media server that shares the user's music,
    # videos and photos with other devices on the network. Not on SnapOS.
    services.gnome.rygel.enable = false;

    systemd.tmpfiles.rules = [
      # The system lives in /etc/snapos; the NixOS path keeps working through a link.
      "L /etc/nixos - - - - /etc/snapos"
    ];
    boot.plymouth.enable = false;
    boot.loader.grub.configurationLimit = 10;
    programs.dconf.profiles.user.databases = [
      { settings."org/gnome/desktop/peripherals/touchpad".disable-while-typing = false; }
    ];

    documentation.nixos.enable = false;
    documentation.doc.enable = false;
    documentation.info.enable = false;

    nix.settings.experimental-features = [ "nix-command" "flakes" ];
    nix.settings.auto-optimise-store = true;
    nix.gc = {
      automatic = true;
      dates = "weekly";
      options = "--delete-older-than 14d";
    };
    zramSwap.enable = true;

    hardware.enableRedistributableFirmware = true;
    hardware.graphics.enable = mkIf config.services.xserver.enable true;
    services.pulseaudio.enable = false;
    security.rtkit.enable = mkIf config.services.xserver.enable true;
    services.pipewire = mkIf config.services.xserver.enable {
      enable = true;
      alsa.enable = true;
      pulse.enable = true;
      wireplumber.enable = true;
    };
    services.fwupd.enable = config.services.xserver.enable;
    services.udisks2.enable = config.services.xserver.enable;
    services.gvfs.enable = mkIf config.services.xserver.enable true;
    services.libinput.touchpad.disableWhileTyping = mkIf config.services.xserver.enable false;
    services.journald.extraConfig = "SystemMaxUse=200M";

    time.timeZone       = mkDefault "UTC";
    i18n.defaultLocale  = mkDefault "en_US.UTF-8";
    console.keyMap      = mkDefault "us";
  };
}
