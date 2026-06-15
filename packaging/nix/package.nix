{ lib
, stdenvNoCC
, makeWrapper
, podman
, coreutils
, gnutar
, xorg
}:

# Installs the launcher + image build context + desktop entry. System
# integration (kernel module, udev, virtual mic) is handled by the NixOS module,
# not by this package. The GPU container image is built at runtime with
# `blucast --build --sdk=...` from a user-provided proprietary SDK.
stdenvNoCC.mkDerivation (finalAttrs: {
  pname = "blucast";
  version = "0.1.0";

  src = ../..;

  nativeBuildInputs = [ makeWrapper ];
  dontConfigure = true;
  dontBuild = true;

  installPhase = ''
    runHook preInstall

    install -Dm755 packaging/common/blucast $out/bin/blucast

    install -Dm644 Containerfile $out/share/blucast/Containerfile
    cp -r app    $out/share/blucast/app
    cp -r assets $out/share/blucast/assets
    install -Dm755 scripts/vcam_watcher.sh $out/share/blucast/scripts/vcam_watcher.sh
    install -Dm755 scripts/vmic_watcher.sh $out/share/blucast/scripts/vmic_watcher.sh

    install -Dm644 packaging/common/blucast.desktop $out/share/applications/blucast.desktop
    install -Dm644 assets/logo.svg $out/share/icons/hicolor/scalable/apps/blucast.svg

    wrapProgram $out/bin/blucast \
      --set BLUCAST_DATADIR $out/share/blucast \
      --prefix PATH : ${lib.makeBinPath [ podman coreutils gnutar xorg.xauth ]}

    runHook postInstall
  '';

  meta = with lib; {
    description = "AI-powered virtual camera and microphone (NVIDIA Maxine)";
    homepage = "https://github.com/Andrei9383/BluCast";
    license = licenses.mit;
    platforms = [ "x86_64-linux" ];
    mainProgram = "blucast";
  };
})
