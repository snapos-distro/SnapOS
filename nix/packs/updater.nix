{ config, lib, pkgs, ... }:

# Updates: the window at login that offers a new release or newer packages,
# and the guard that undoes an update that does not reach the login screen.
with lib;
{
  config = mkIf config.snapos.packs.updater.enable {
    # git: the update check asks it for the newest package list
    environment.systemPackages = [ pkgs.snapos-tools pkgs.git ] ++ optional config.services.xserver.enable pkgs.snaphelper;

    # An update is only made the default for the next boot. At boot the guard
    # lets a new system start once; it is approved when a normal user logs in,
    # and undone (back to the previous generation) if that never happens.
    systemd.services.snapos-update-guard = {
      description = "SnapOS update guard: undo an update that did not come up";
      wantedBy = [ "multi-user.target" ];
      before = [ "display-manager.service" ];
      after = [ "local-fs.target" ];
      path = [ config.nix.package pkgs.systemd pkgs.coreutils ];
      serviceConfig = { Type = "oneshot"; ExecStart = "${pkgs.snapos-tools}/bin/snapos guard boot"; };
    };
    systemd.services.snapos-update-approve = {
      description = "SnapOS update guard: approve the new system once someone logs in";
      wantedBy = [ "graphical.target" ];
      after = [ "display-manager.service" ];
      path = [ config.nix.package pkgs.systemd pkgs.coreutils ];
      serviceConfig = { Type = "simple"; ExecStart = "${pkgs.snapos-tools}/bin/snapos guard approve"; };
    };

    # At every login, snapupdate looks for a newer release and offers it.
    environment.etc = mkIf config.services.xserver.enable {
      "xdg/autostart/snapupdate.desktop".text = ''
        [Desktop Entry]
        Type=Application
        Name=SnapOS Update
        Exec=snapupdate --autostart
        Terminal=false
        X-GNOME-Autostart-enabled=true
      '';
    };
    systemd.tmpfiles.rules = [
      "d /var/lib/snapos/update 0755 root root -"
      "d /var/lib/snapos/update/work 0700 root root -"
    ];
  };
}
