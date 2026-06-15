{
  description = "BluCast — AI-powered virtual camera and microphone (NVIDIA Maxine)";

  inputs.nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";

  outputs = { self, nixpkgs }:
    let
      forAllSystems = nixpkgs.lib.genAttrs [ "x86_64-linux" ];
    in
    {
      packages = forAllSystems (system: {
        default = nixpkgs.legacyPackages.${system}.callPackage ./packaging/nix/package.nix { };
      });

      # NixOS module: declaratively wires v4l2loopback, the PipeWire virtual mic,
      # udev, podman and the app. Enable with `programs.blucast.enable = true;`.
      nixosModules.blucast = import ./packaging/nix/module.nix self;
      nixosModules.default = self.nixosModules.blucast;
    };
}
