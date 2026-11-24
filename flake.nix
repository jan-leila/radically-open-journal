{
  description = "Radically Open Journal - an RO-DBT diary card app for Pebble";

  # Prebuilt Pebble SDK tools, so entering the shell does not compile the emulator.
  nixConfig = {
    extra-substituters = [ "https://pebble.cachix.org" ];
    extra-trusted-public-keys = [
      "pebble.cachix.org-1:aTqwT2hR6lGggw/rPISRcHZctDv2iF7ewsVxf3Hq6ow="
    ];
  };

  inputs = {
    pebble.url = "github:pebble-dev/pebble.nix";
    nixpkgs.follows = "pebble/nixpkgs";
    flake-utils.url = "github:numtide/flake-utils";
  };

  outputs =
    {
      pebble,
      nixpkgs,
      flake-utils,
      ...
    }:
    flake-utils.lib.eachDefaultSystem (
      system:
      let
        pkgs = nixpkgs.legacyPackages.${system};
      in
      {
        devShells.default = pebble.pebbleEnv.${system} {
          # The emulator loads SDL3 (and through it ALSA) at runtime. Without this it finds
          # either nothing or the system copies, which are built against another glibc.
          shellHook = ''
            export LD_LIBRARY_PATH=${pkgs.lib.makeLibraryPath [ pkgs.sdl3 pkgs.alsa-lib ]}''${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}
          '';
        };
      }
    );
}
