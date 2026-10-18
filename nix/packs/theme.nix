{ config, lib, pkgs, ... }:

# The SnapOS look: red GTK theme and icons, dark or light, the wallpapers,
# the login screen, the boot menu picture and Snappy in fastfetch.
with lib;
let
  cfg = config.snapos;
  light = cfg.appearance == "light";
  mode = if light then "light" else "dark";

  redTheme = pkgs.colloid-gtk-theme.override {
    themeVariants = [ "red" ];
    colorVariants = [ "light" "dark" ];
  };
  redIcons = pkgs.papirus-icon-theme.override { color = "red"; };

  # Colloid's light theme ships a dark gtk-dark.css. Any program that asks for
  # the dark variant would draw white text on the light desktop, so in this copy
  # the dark variant is the light one too.
  lightTheme = pkgs.runCommand "snapos-light-gtk-theme" { } ''
    d=$out/share/themes/SnapOS-Light
    mkdir -p $out/share/themes
    cp -rL ${redTheme}/share/themes/Colloid-Red-Light $d
    chmod -R u+w $d
    cp $d/gtk-3.0/gtk.css $d/gtk-3.0/gtk-dark.css
    if [ -e $d/gtk-4.0/gtk.css ]; then cp $d/gtk-4.0/gtk.css $d/gtk-4.0/gtk-dark.css; fi
    sed -i 's/^Name=.*/Name=SnapOS-Light/' $d/index.theme
  '';
  gtkTheme  = if light then "SnapOS-Light" else "Colloid-Red-Dark";
  gtkPackage = if light then lightTheme else redTheme;
  iconName  = if light then "Papirus-Light" else "Papirus-Dark";
  scheme    = if light then "prefer-light" else "prefer-dark";

  wallpaper = "file://${pkgs.snapos-backgrounds}/share/backgrounds/snapos/snapos-${mode}.jpeg";
  loginBackground =
    if light then ../../branding/wallpapers/snapos-light.jpeg
    else ../../branding/wallpapers/snapos-default.jpeg;
  primaryColor   = if light then "#ECE9E5" else "#141110";
  secondaryColor = if light then "#FFFFFF" else "#000000";

  # The shield is white on a dark desktop and black on a light one.
  shieldIcons = pkgs.runCommand "snapguard-${mode}-icons" { } ''
    for s in 16 24 32 48 64 128 256 512; do
      install -Dm644 ${../../branding/icons}/snapguard-${mode}-$s.png \
        $out/share/icons/hicolor/''${s}x''${s}/apps/snapguard.png
    done
  '';
in {
  config = mkIf cfg.packs.theme.enable {
    environment.etc."snapos/theme".text = cfg.theme;
    environment.etc."snapos/appearance".text = cfg.appearance;
    environment.etc."snapos/fastfetch.jsonc".source = ../../branding/fastfetch/config.jsonc;
    environment.etc."snapos/logo.txt".source        = ../../branding/fastfetch/snapos-logo.txt;
    environment.etc."snapos/mascot.txt".source      = ../../branding/snappy-mascot.txt;
    # The same Snappy drawn with the characters the Linux console font has.
    environment.etc."snapos/mascot-console.txt".source = ../../branding/snappy-console.txt;

    environment.systemPackages = [ pkgs.fastfetch ]
      ++ optionals config.services.xserver.enable ([ redTheme redIcons (hiPrio shieldIcons) pkgs.snapos-backgrounds ] ++ optional light lightTheme);

    boot.loader.grub.splashImage = ../../branding/splash/boot-800x600.png;
    boot.loader.grub.backgroundColor = "#141110";

    services.xserver.displayManager.lightdm.background = loginBackground;
    # the same login screen on every desktop (only Budgie turns it on by itself)
    services.xserver.displayManager.lightdm.greeters.slick = mkIf config.services.xserver.enable {
      enable = true;
      theme = { name = gtkTheme; package = gtkPackage; };
      iconTheme = { name = iconName; package = redIcons; };
    };

    programs.dconf.enable = true;
    programs.dconf.profiles.user.databases = [
      {
        settings = {
          "org/gnome/desktop/background" = {
            picture-uri = wallpaper;
            picture-uri-dark = wallpaper;
            picture-options = "zoom";
            primary-color = primaryColor;
            secondary-color = secondaryColor;
          };
          "org/gnome/desktop/screensaver" = {
            picture-uri = wallpaper;
            picture-options = "zoom";
            primary-color = primaryColor;
          };
          "org/gnome/desktop/interface" = {
            gtk-theme = gtkTheme;
            icon-theme = iconName;
            color-scheme = scheme;
          };
        };
      }
    ];
  };
}
