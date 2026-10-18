{ writeShellScriptBin, symlinkJoin }:

# The NixOS tools under SnapOS names. Each one runs the NixOS tool; the system
# lives in /etc/snapos, and /etc/nixos points to it so NixOS finds it too.
let
  generate = writeShellScriptBin "snapos-generate-config" ''
    # snapos-generate-config [--root DIR]: writes hardware-configuration.nix
    # (and a first configuration.nix) in DIR/etc/snapos.
    set -e
    root=/
    args=("$@")
    while [ $# -gt 0 ]; do [ "$1" = --root ] && root=$2; shift; done
    nixos-generate-config "''${args[@]}"
    if [ -d "$root/etc/nixos" ] && [ ! -L "$root/etc/nixos" ]; then
      mkdir -p "$root/etc/snapos"
      for f in "$root"/etc/nixos/*; do
        [ -e "$root/etc/snapos/$(basename "$f")" ] || mv "$f" "$root/etc/snapos/"
      done
      rm -rf "$root/etc/nixos"
      ln -s snapos "$root/etc/nixos"
    fi
    echo "SnapOS: your system files are in $root/etc/snapos"
  '';
  install = writeShellScriptBin "snapos-install" ''
    # snapos-install --flake /mnt/etc/snapos#NAME: nixos-install. The lock file
    # is written first: nixos-install would write it mid-way and then find the
    # folder changed.
    flake=
    prev=
    for a in "$@"; do
      [ "$prev" = --flake ] && flake=$a
      case "$a" in --flake=*) flake=''${a#--flake=} ;; esac
      prev=$a
    done
    dir=''${flake%%#*}
    dir=''${dir#path:}
    if [ -n "$dir" ] && [ -f "$dir/flake.nix" ] && [ ! -f "$dir/flake.lock" ]; then
      nix --extra-experimental-features "nix-command flakes" flake lock "path:$dir"
    fi
    exec nixos-install "$@"
  '';
  enter = writeShellScriptBin "snapos-enter" ''
    # snapos-enter: a shell inside the installed system (nixos-enter)
    exec nixos-enter "$@"
  '';
  rebuild = writeShellScriptBin "snapos-rebuild" ''
    # snapos-rebuild switch|boot|build [...]: nixos-rebuild, with the system in
    # /etc/snapos named after this computer unless --flake is given
    for a in "$@"; do case "$a" in --flake|--flake=*) exec nixos-rebuild "$@" ;; esac; done
    exec nixos-rebuild "$@" --flake "/etc/snapos#$(hostname)"
  '';
in
symlinkJoin { name = "snapos-nix-tools"; paths = [ generate install enter rebuild ]; }
