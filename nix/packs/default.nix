{ config, lib, ... }:

# The SnapOS packs. Each one is off until it is turned on, so they can be
# added to any NixOS system, one by one:
#
#   snapos.packs.theme.enable = true;
#
# The full SnapOS (nix/modules/snapos.nix) turns them all on.
with lib;
let pack = name: what: mkEnableOption "the SnapOS ${name} pack: ${what}"; in
{
  imports = [ ./theme.nix ./commands.nix ./snapguard.nix ./snapweb.nix ./debian.nix ./helper.nix ./updater.nix ../modules/desktop.nix ];

  options.snapos = {
    packs = {
      theme.enable     = pack "theme" "the red look, icons, wallpapers, login screen and Snappy in the terminal";
      desktops.enable  = pack "desktops" "Budgie, KDE Plasma, Xfce or Hyprland set up the SnapOS way (snapos.desktop)";
      commands.enable  = pack "commands" "the snapos command (find, add, remove, rebuild, rollback...) and the apt helper";
      snapguard.enable = pack "snapguard" "the SnapGuard antivirus with ClamAV, in real time";
      snapweb.enable   = pack "snapweb" "SnapWeb, Firefox with the SnapOS look";
      debian.enable    = pack "debian" "the Debian layer for .deb programs (snap-deb)";
      helper.enable    = pack "helper" "SnapHelper, the tour at the first login";
      updater.enable   = pack "updater" "the update window and the update guard";
    };
    theme = mkOption { type = types.str; default = "snappy-red"; };
    appearance = mkOption {
      type = types.enum [ "dark" "light" ];
      default = "dark";
      description = "Dark or light desktop. The installer asks for it; change it any time and run snapos rebuild.";
    };
    security = {
      antivirus  = mkOption { type = types.enum [ "snapguard" "none" ]; default = "snapguard"; };
      onInfected = mkOption { type = types.enum [ "contain" "warn" ]; default = "contain"; };
    };
  };

  # SnapOS keeps its state in /var/lib/snapos (readable: the login check and
  # `snapos log` run as the user; what is private in it keeps its own mode).
  config = mkIf (any (p: p.enable) (attrValues config.snapos.packs)) {
    systemd.tmpfiles.rules = [ "d /var/lib/snapos 0755 root root -" ];
    # the packs version, which `snapos update` compares with the newest release
    environment.etc."snapos-packs".text = builtins.readFile ../../VERSION;
  };
}
