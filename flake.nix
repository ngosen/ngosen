{
  description = "Ngó Sen, a Vietnamese input method for Fcitx5";

  inputs.nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";

  outputs =
    { self, nixpkgs }:
    let
      systems = [
        "x86_64-linux"
        "aarch64-linux"
      ];
      forAllSystems = f: nixpkgs.lib.genAttrs systems (system: f nixpkgs.legacyPackages.${system});
    in
    {
      packages = forAllSystems (pkgs: rec {
        fcitx5-ngosen = pkgs.callPackage ./nix/package.nix { src = self; };
        default = fcitx5-ngosen;
      });

      overlays.default = final: prev: {
        fcitx5-ngosen = final.callPackage ./nix/package.nix { src = self; };
      };

      checks = forAllSystems (
        pkgs:
        let
          inherit (self.packages.${pkgs.stdenv.hostPlatform.system}) fcitx5-ngosen;
        in
        {
          inherit fcitx5-ngosen;
          x11-smoke = pkgs.callPackage ./nix/x11-smoke.nix {
            fcitx5-with-addons = pkgs.qt6Packages.fcitx5-with-addons.override {
              addons = [ fcitx5-ngosen ];
            };
          };
        }
      );

      formatter = forAllSystems (pkgs: pkgs.nixfmt);
    };
}
