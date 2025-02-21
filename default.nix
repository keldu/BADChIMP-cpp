{ pkgs ? import <nixpkgs> {}
, stdenv ? pkgs.clangStdenv
}:


pkgs.callPackage .nix/derivation.nix {
  inherit stdenv;
  python3 = pkgs.python312;
  python3Packages = pkgs.python312Packages;
}
