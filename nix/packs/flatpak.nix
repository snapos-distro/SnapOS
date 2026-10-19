{ config, lib, pkgs, ... }:

# Flatpak with Flathub, for a program that only Flathub has. Off by default:
# SnapOS's programs come from Nix (Carapace, snapos add).
with lib;
{
  config = mkIf (config.snapos.packs.flatpak.enable && config.services.xserver.enable) {
    services.flatpak.enable = true;
    xdg.portal.enable = true;
    xdg.portal.extraPortals = [ pkgs.xdg-desktop-portal-gtk ];
    xdg.portal.config.common.default = mkDefault "*";

    systemd.services.flatpak-flathub = {
      wantedBy = [ "multi-user.target" ];
      wants = [ "network-online.target" ];
      after = [ "network-online.target" ];
      path = [ pkgs.flatpak ];
      script = ''
        until flatpak remote-add --system --if-not-exists flathub https://dl.flathub.org/repo/flathub.flatpakrepo; do
          sleep 30
        done
        until flatpak update --system --appstream; do
          sleep 30
        done
      '';
    };
  };
}
