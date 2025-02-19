{ pkgs ? import <nixpkgs> {}
, stdenv ? pkgs.gccStdenv
}:


pkgs.callPackage .nix/derivation.nix {
  inherit stdenv;
  python3 = pkgs.python312;
  python3Packages = pkgs.python312Packages;
}
