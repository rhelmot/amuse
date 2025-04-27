final: prev: let 
  athena = final.fetchFromGitHub {
    owner = "libAthena";
    repo = "athena";
    rev = "fa346ace4774e4ac66644be6d11908a80f0151b6";
    hash = "sha256-qlUabtcOwJ867nQ9fwBQRI/5CjALDHpoF0Uryr0qgrM=";
    deepClone = true;
    fetchSubmodules = true;
  };
in {
  atdna = final.callPackage ({
    stdenv,
    cmake,
    llvmPackages_15,
  }:
  stdenv.mkDerivation {
    pname = "atdna";
    version = "devel";
    src = athena;
    sourceRoot = "source/atdna";

    postPatch = ''
      substituteInPlace CMakeLists.txt --replace-fail LLVM_INCLUDE_DIR CLANG_INCLUDE_DIRS
    '';

    postInstall = ''
      export wrapped=$out/bin/.atdna-wrapped
      export defaultArgs="-I${llvmPackages_15.clang}/resource-root/include";
      NEXT=""
      for word in $(cat ${llvmPackages_15.clang}/nix-support/libcxx-cxxflags ${llvmPackages_15.clang}/nix-support/libc-cflags); do
        if [[ -n "$NEXT" ]]; then
          defaultArgs+=" $NEXT $word"
          NEXT=""
          continue
        fi
        case "$word" in
          -isystem|-I)
            NEXT="$word"
            ;;
          -idirafter)
            NEXT="-isystem"
            ;;
          -isystem=*|-I*)
            defaultArgs+=" $word"
            ;;
          -idirafter=*)
            defaultArgs+=" -isystem=''${word#-idirafter=}"
            ;;
        esac
      done
      mv $out/bin/atdna $wrapped
      substituteAll ${./atdna-wrapper.sh} $out/bin/atdna
      chmod +x $out/bin/atdna
    '';

    nativeBuildInputs = [
      cmake
    ];
    buildInputs = [
      llvmPackages_15.llvm
      llvmPackages_15.clang.cc
    ];
  }) {};

  bintoc = final.callPackage ({
    fetchurl,
    zlib,
    stdenv,
  }: stdenv.mkDerivation {
    name = "bintoc";
    src = fetchurl {
      url = "https://raw.githubusercontent.com/AxioDL/metaforce/394ed34673be4f54c9fe947466295f8008b4edcb/bintoc/bintoc.c";
      hash = "sha256-qd6mGCf0kdeFT1vcpufbToMqk7uBss1lOFiM0Oa1GPo=";
    };

    buildInputs = [
      zlib
    ];

    unpackPhase = ":";

    buildPhase = ''
      mkdir -p $out/bin
      $CC -o $out/bin/bintoc $src -lz
    '';
  }) {};

  amuse = final.callPackage ({
    lib,
    stdenv,
    buildPackages,
    fetchFromGitHub,
    fetchurl,
    zlib,
    cmake,
    qt6,
    atdna,
    bintoc,
  }:

  let
    boo = fetchFromGitHub {
      owner = "AxioDL";
      repo = "boo";
      rev = "e9b2c5f96c4d48e57ca5b8eb0aa41504f4a81672";
      hash = "sha256-m9pFy8qw3vdjJQ3qDVeuSMCqDAxtt5v9c9zhsQBpdi4=";
      deepClone = true;
      fetchSubmodules = true;
    };
  in stdenv.mkDerivation {
    pname = "amuse";
    version = "devel";
    #src = fetchFromGitHub {
    #  owner = "AxioDL";
    #  repo = "amuse";
    #  rev = "ad9bc96af472dae03fde215163446bdec80be67f";
    #  hash = "sha256-LF834v7fi7Idrm1Vdx5U8AhhvtTI8MGj33iwk/kXr28=";
    #};
    src = ./.;

    postUnpack = ''
      rm -rf $sourceRoot/{boo,athena}
      cp -r ${boo} $sourceRoot/boo
      cp -r ${athena} $sourceRoot/athena
      chmod -R +w $sourceRoot
    '';

    postPatch = ''
      find . -type f -print0 | xargs -0 sed -E -i -e 's/#include\s*<([^/]*\.h)>/#include <\L\1>/g'
      substituteInPlace athena/atdna/atdnaHelpers.cmake \
        --replace-fail '$<TARGET_FILE:atdna>' 'atdna' \
        --replace-fail 'DEPENDS atdna' 'DEPENDS'
      substituteInPlace athena/include/athena/Dir.hpp \
        --replace-fail "using mode_t = int;" "#include <sys/types.h>"
      substituteInPlace athena/src/athena/FileInfo.cpp \
        --replace-fail "ifdef _MSC_VER" "if defined(_MSC_VER) || defined(__MINGW32__)" \
        --replace-fail "ifdef _WIN32" "if defined(_WIN32) || defined(__MINGW32__)" \
        --replace-fail "ifndef _WIN32" 'if !defined(_WIN32) || defined(__MINGW32__)'
      substituteInPlace boo/lib/win/ApplicationWin32.cpp \
        --replace-fail 'compare(L' 'compare('
      sed -E -i -e '/stat.h/i #include <sys/types.h>' athena/include/athena/Global.hpp
      sed -E -i -e '/dirent.h/i #include <utime.h>' athena/src/athena/FileInfo.cpp
      sed -E -i -e '/pD3DPERF_EndEvent/a #include "mingw_extras.h"' boo/lib/graphicsdev/D3D11.cpp
      sed -E -i -e '/IID IID/d' -e '/CLSID CLSID/d' boo/lib/audiodev/WASAPI.cpp
      #sed -E -i -e '/optick/a #include <ksguid.h>\n#include <mmreg.h>\n#include <ks.h>' boo/lib/audiodev/WASAPI.cpp
      cp ${./mingw_extras.hpp} boo/lib/graphicsdev/mingw_extras.h
      substituteInPlace boo/CMakeLists.txt \
        --replace-fail Xinput xinput \
        --replace-fail Winusb winusb \
        --replace-fail Winmm winmm \
        --replace-fail Shlwapi shlwapi \
        --replace-fail Setupapi setupapi \
        --replace-fail Imm32 imm32 \
        --replace-fail Hid 'hid dbghelp' \
        --replace-fail 'add_subdirectory(test)' ""
    '';

    preBuild = ''
      mkdir -p $TMP/bin
      ${buildPackages.stdenv.cc}/bin/${buildPackages.stdenv.cc.targetPrefix}cc ../Editor/platforms/freedesktop/mkqticon.c -o $TMP/bin/amuse-mkqticon.exe
      export PATH="$PATH:$TMP/bin"
    '';

    env.NIX_CFLAGS_COMPILE = "-DS_ISLNK(x)=0 -Dutimes=utime -DINITGUID";
    dontWrapQtApps = true;

    depsBuildBuild = [
      atdna
    ];

    nativeBuildInputs = [
      buildPackages.stdenv.cc
      cmake
      bintoc
      qt6.qttools
    ];

    buildInputs = [
      qt6.qtbase
      qt6.qtsvg
      qt6.qtdeclarative
    ];
  }) {};

  amuse-dist = final.callPackage ({
    runCommand,
    amuse,
    zip,
    qt6,
  }:
  runCommand "amuse-dist.zip" {
    nativeBuildInputs = [
      zip
    ];
  } ''
    mkdir -p bin/lib/qt-6/plugins/platforms
    cp ${qt6.qtbase}/lib/qt-6/plugins/platforms/* bin/lib/qt-6/plugins/platforms
    cp ${amuse}/bin/* bin
    zip -r $out bin
  '') {};
}
