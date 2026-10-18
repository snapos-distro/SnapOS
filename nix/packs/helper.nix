{ config, lib, pkgs, ... }:

# SnapHelper opens once, the first time each user logs in.
with lib;
{
  config = mkIf (config.snapos.packs.helper.enable && config.services.xserver.enable) {
    environment.systemPackages = [ pkgs.snaphelper ];
    environment.etc."xdg/autostart/snaphelper.desktop".text = ''
      [Desktop Entry]
      Type=Application
      Name=SnapHelper
      Exec=snaphelper --first-run
      Terminal=false
      X-GNOME-Autostart-enabled=true
    '';
    # SnapHelper's animations live in share/snapos; desktops that do not link
    # the whole share tree (Hyprland) still need it.
    environment.pathsToLink = [ "/share/snapos" ];
  };
}
