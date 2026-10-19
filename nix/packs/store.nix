{ config, lib, options, pkgs, ... }:

# Carapace, the SnapOS store. Installing asks for the password through
# polkit (pkexec): every desktop brings an agent for that window.
with lib;
{
  config = mkIf (config.snapos.packs.store.enable && config.services.xserver.enable) {
    environment.systemPackages = [ pkgs.carapace ];
    security.polkit.enable = true;
    # every program in nixpkgs can be installed, the unfree ones too (when the
    # system makes its own package set; a given one, as in the VM tests, is kept)
    nixpkgs.config = mkIf (!options.nixpkgs.pkgs.isDefined) { allowUnfree = true; };
    # Steam and many games are 32-bit: their graphics libraries, on x86 PCs
    hardware.graphics.enable32Bit = mkIf pkgs.stdenv.hostPlatform.isx86_64 (mkDefault true);
    # Carapace's scenes live in share/carapace; desktops that do not link the
    # whole share tree (Hyprland) still need it.
    environment.pathsToLink = [ "/share/carapace" ];
  };
}
