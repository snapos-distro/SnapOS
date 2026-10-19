# /etc/snapos/configuration.nix: your whole system, written by you.
{ config, pkgs, ... }:

{
  imports = [ ./hardware-configuration.nix ];

  # Starting the computer (UEFI)
  boot.loader.systemd-boot.enable = true;
  boot.loader.efi.canTouchEfiVariables = true;

  # Name, network, time and language
  networking.hostName = "mypc";
  networking.networkmanager.enable = true;
  time.timeZone = "America/Sao_Paulo";
  i18n.defaultLocale = "pt_BR.UTF-8";
  console.keyMap = "br-abnt2";

  # You
  users.users.ana = {
    isNormalUser = true;
    extraGroups = [ "wheel" "networkmanager" ];
  };

  # A desktop: Xfce here (or none at all: delete these four lines)
  services.xserver.enable = true;
  services.xserver.desktopManager.xfce.enable = true;
  services.xserver.displayManager.lightdm.enable = true;
  services.xserver.xkb.layout = "br";

  # Programs (snapos add and snapos remove edit this list for you)
  # snapos:packages:begin
  environment.systemPackages = with pkgs; [
    firefox
    git
    nano
  ];
  # snapos:packages:end

  # The SnapOS packs you want
  snapos.packs.theme.enable = true;      # the red look and Snappy
  snapos.packs.commands.enable = true;   # snapos find, add, rollback...
  snapos.packs.snapguard.enable = true;  # the antivirus
  snapos.packs.debian.enable = true;     # .deb programs
  snapos.packs.updater.enable = true;    # updates of the packs
  snapos.packs.store.enable = true;      # Carapace, the store
  # snapos.packs.flatpak.enable = true;  # Flatpak with Flathub, if you need it

  nix.settings.experimental-features = [ "nix-command" "flakes" ];
  system.stateVersion = "26.05";
}
