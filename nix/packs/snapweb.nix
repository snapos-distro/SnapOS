{ config, lib, pkgs, ... }:

# SnapWeb: Firefox with the SnapOS look, the default browser.
with lib;
{
  config = mkIf config.snapos.packs.snapweb.enable {
    environment.systemPackages = [ pkgs.snapweb ];
    xdg.mime.defaultApplications = {
      "text/html" = "firefox.desktop";
      "x-scheme-handler/http" = "firefox.desktop";
      "x-scheme-handler/https" = "firefox.desktop";
      "x-scheme-handler/about" = "firefox.desktop";
      "x-scheme-handler/unknown" = "firefox.desktop";
    };
  };
}
