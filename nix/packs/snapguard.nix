{ config, lib, pkgs, ... }:

# SnapGuard: ClamAV, the SnapOS deny-list and a trust list, a window, and a
# watcher that scans Downloads and USB drives in real time.
with lib;
let cfg = config.snapos; on = cfg.packs.snapguard.enable && cfg.security.antivirus == "snapguard"; in
{
  config = mkIf on {
    environment.systemPackages = [ pkgs.snapguard pkgs.snapos-tools pkgs.libnotify ];

    # SnapGuard in real time: at login, snapguard-watch starts scanning every
    # new file in Downloads and every USB drive that is plugged in.
    environment.etc."xdg/autostart/snapguard-watch.desktop".text = ''
      [Desktop Entry]
      Type=Application
      Name=SnapGuard real-time protection
      Exec=snapguard-watch
      Terminal=false
      X-GNOME-Autostart-enabled=true
    '';

    services.clamav.daemon.enable = true;
    services.clamav.updater.enable = true;
    # SnapGuard runs as the user, so every user must be able to reach clamd.
    services.clamav.daemon.settings.LocalSocketMode = "666";
    # Tell clamd to reload when freshclam has new signatures.
    services.clamav.updater.settings.NotifyClamd = "/etc/clamav/clamd.conf";
    # The first signature download needs the network. If it is not there yet,
    # try again instead of leaving the defender off until the next reboot.
    systemd.services.clamav-daemon = {
      # If the daemon fails (no database yet), systemd would remove /run/clamav
      # and with it the socket file that clamd is reached through.
      serviceConfig = { Restart = "on-failure"; RestartSec = "60s"; RuntimeDirectoryPreserve = "yes"; };
      unitConfig.StartLimitIntervalSec = 0;
      # NixOS makes the daemon wait for freshclam, so the boot would wait for the
      # signature download. The daemon starts on its own and retries until the
      # timer below has fetched the database.
      after = mkForce [ "clamav-daemon.socket" ];
      wants = mkForce [ ];
    };
    systemd.timers.clamav-freshclam.timerConfig.OnBootSec = "2min";

    environment.etc."snapos/onInfected".text = cfg.security.onInfected;
    environment.etc."snapos/signatures.txt".source = ../../security/signatures.txt;
    systemd.tmpfiles.rules = [
      "f /etc/snapos/allow.txt 0644 root root -"
      "d /var/lib/snapos/quarantine 0700 root root -"
    ];
  };
}
