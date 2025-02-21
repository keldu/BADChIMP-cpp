{ lib
, stdenv
, cmake
, openmpi
, eigen
, python3
, python3Packages
}:

stdenv.mkDerivation {
  pname = "BADChIMP-cpp";
  version = "0.0.0";

  src = ./..;

  nativeBuildInputs = [
    cmake
  ];

  buildInputs = [
    openmpi
    eigen
    python3
    python3Packages.numpy
    python3Packages.matplotlib
  ];
  
  cmakeFlags = [
  ];

  doCheck = true;

  postInstall = ''
    patchShebangs $out/python/
  '';
}
