{ config, lib, pkgs, ... }:

# The snapos command: find, add, remove, shell, diff, rebuild, rollback,
# generations, gc, log, doctor; snapctl; and apt, which explains the SnapOS way.
with lib;
{
  config = mkIf config.snapos.packs.commands.enable {
    environment.systemPackages = [ pkgs.snapos-tools pkgs.snapos-nix-tools pkgs.git ];
    # the system lives in /etc/snapos; NixOS finds it through /etc/nixos
    systemd.tmpfiles.rules = [ "L /etc/nixos - - - - /etc/snapos" ];
    nix.settings.experimental-features = [ "nix-command" "flakes" ];

    # `nix-env -i` looks like apt, but what it installs is not in the system
    # description: rebuilds do not know it, updates skip it and it keeps disk
    # space alive. The shell explains and offers the SnapOS way.
    programs.bash.interactiveShellInit = ''
      nix-env() {
        case " $* " in
          *" -i"*|*" --install"*)
            echo "SnapOS: nix-env would install this outside your system description:"
            echo "  rebuilds and updates would not know about it."
            echo "    snapos add NAME      install it (declared, part of the system)"
            echo "    snapos shell NAME    use it now without installing"
            echo "  To use nix-env anyway: command nix-env $*"
            return 1 ;;
        esac
        command nix-env "$@"
      }
    '';

  };
}
