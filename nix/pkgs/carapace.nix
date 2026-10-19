{ lib, stdenv, pkgs, writeText, pkg-config, wrapGAppsHook3, gtk3, src }:

# Carapace, the SnapOS store: the programs from Nix, installed with
# `snapos add` through pkexec, and Snappy's scenes while it works.
let
  # the first page keeps only the programs that exist for this computer
  # (no Chrome or Spotify on ARM)
  lines = builtins.filter (l: l != "" && !lib.hasPrefix "#" l)
    (lib.splitString "\n" (builtins.readFile (src + "/branding/carapace/apps.tsv")));
  available = l:
    let p = lib.attrByPath (lib.splitString "." (builtins.head (lib.splitString "\t" l))) null pkgs;
    in p != null && lib.meta.availableOn stdenv.hostPlatform p;
  kept = builtins.filter available lines;
  apps = writeText "apps.tsv" (lib.concatStringsSep "\n" kept + "\n");
in
stdenv.mkDerivation {
  pname = "carapace";
  version = "0.2";
  inherit src;

  nativeBuildInputs = [ pkg-config wrapGAppsHook3 ];
  buildInputs = [ gtk3 ];

  buildPhase = ''
    runHook preBuild
    make bin/carapace CARAPACE_DATA=$out/share/carapace
    runHook postBuild
  '';

  installPhase = ''
    runHook preInstall
    install -Dm755 bin/carapace $out/bin/carapace
    install -Dm644 ${apps} $out/share/carapace/apps.tsv
    xz -dc branding/carapace/index.tsv.xz > $out/share/carapace/index.tsv
    for g in branding/carapace/scenes/*.gif; do install -Dm644 "$g" $out/share/carapace/scenes/$(basename "$g"); done
    install -Dm644 branding/carapace/carapace.desktop $out/share/applications/carapace.desktop
    install -Dm644 branding/carapace/carapace.svg $out/share/icons/hicolor/scalable/apps/carapace.svg
    for s in 16 24 32 48 64 128 256 512; do
      install -Dm644 branding/carapace/icons/carapace-$s.png $out/share/icons/hicolor/''${s}x''${s}/apps/carapace.png
    done
    runHook postInstall
  '';

  passthru = { inherit apps; programs = map (l: builtins.head (lib.splitString "\t" l)) kept; };

  meta = {
    description = "Carapace, the SnapOS store";
    mainProgram = "carapace";
    platforms = [ "x86_64-linux" "aarch64-linux" ];
  };
}
