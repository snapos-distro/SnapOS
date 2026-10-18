{ config, pkgs, lib, modulesPath, self, ... }:

# The SnapOS minimal USB: the NixOS minimal ISO, a terminal and nothing
# more. You build your own system by hand, the NixOS way, and add the SnapOS
# packs you want to it. The handbook (https://snapos-distro.github.io) walks
# through every step. The SnapOS source is on the USB in /etc/snapos-src, so
# the packs can be used without downloading them.
{
  imports = [
    (modulesPath + "/installer/cd-dvd/installation-cd-minimal.nix")
    ./packs
  ];

  snapos.packs.theme.enable = true;      # Snappy in fastfetch
  snapos.packs.commands.enable = true;   # snapos find, snapos shell...
  networking.networkmanager.enable = true;
  nix.settings.experimental-features = [ "nix-command" "flakes" ];
  i18n.supportedLocales = [ "all" ];

  environment.systemPackages = [ pkgs.parted pkgs.git pkgs.pciutils pkgs.snapos-nix-tools ];
  environment.etc."snapos-build".text = self.shortRev or self.dirtyShortRev or "unknown";
  environment.etc."snapos-src".source = ../.;

  # What you see after it starts: where you are and the first step.
  services.getty.helpLine = lib.mkForce ''

    SnapOS minimal: a terminal. Build your system by hand, the NixOS way,
    and add the SnapOS packs you want. The handbook shows every step:
        https://snapos-distro.github.io/handbook/
    Wi-Fi: nmtui      Disks: lsblk      Editor: nano
  '';

  boot.kernelParams = [ "console=tty0" ]
    ++ lib.optionals pkgs.stdenv.hostPlatform.isx86_64 [ "console=ttyS0,115200" ]
    ++ lib.optionals pkgs.stdenv.hostPlatform.isAarch64 [ "console=ttyAMA0,115200" ];

  isoImage.isoName = lib.mkForce (if pkgs.stdenv.hostPlatform.isAarch64 then "snapos-minimal-aarch64.iso" else "snapos-minimal.iso");
  isoImage.volumeID = lib.mkForce "SNAPOS_MINIMAL";
  isoImage.appendToMenuLabel = " SnapOS minimal";
  isoImage.splashImage = ../branding/splash/boot-800x600.png;
  isoImage.efiSplashImage = ../branding/splash/efi-1920x1080.png;
  isoImage.grubTheme = null;

  system.stateVersion = "26.05";
}
