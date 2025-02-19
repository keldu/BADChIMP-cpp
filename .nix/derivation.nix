{ lib
, stdenv
, cmake
, openmpi
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
    python3
    python3Packages.numpy
    python3Packages.matplotlib
  ];
  
  cmakeFlags = [
    "DLBMAIN:STRING=./input"
  ];

  installPhase = ''
    mkdir -p $out
    mkdir -p $out/bin
    mkdir -p $out/python
    cp bin/bdchmp $out/bin
    cp $src/PythonScripts/*.py $out/python
  '';
}
