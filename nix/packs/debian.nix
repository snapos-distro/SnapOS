{ config, lib, pkgs, ... }:

# The Debian layer: every .deb declared in /etc/snapos/debs is installed by
# `snap-deb sync` in a real Debian under /var/lib/snapdeb, and its menu entries
# and commands are exported from there. SnapGuard scans every .deb first.
with lib;
let debLayer = "/var/lib/snapdeb"; in
{
  config = mkIf config.snapos.packs.debian.enable {
    environment.systemPackages = [ pkgs.snapos-tools pkgs.bubblewrap pkgs.debootstrap pkgs.dpkg ];
    # Debian's archive keys: the Debian layer is only created from a verified archive.
    environment.etc."snapos/debian-archive-keyring.gpg".source = ../../security/debian-archive-keyring.gpg;
    # Programs from the Debian layer show up in the menu and on the PATH.
    environment.sessionVariables.XDG_DATA_DIRS = [ "${debLayer}/exports/share" ];
    environment.extraInit = ''
      export PATH="$PATH:${debLayer}/exports/bin"
    '';
    # Lets programs built for other distributions find their libraries.
    programs.nix-ld.enable = true;
    xdg.mime.defaultApplications = {
      "application/vnd.debian.binary-package" = "snap-deb.desktop";
      "application/x-deb" = "snap-deb.desktop";
      "application/x-debian-package" = "snap-deb.desktop";
    };
    systemd.tmpfiles.rules = [ "d ${debLayer} 0755 root root -" ];
  };
}
