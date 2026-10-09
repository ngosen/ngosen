{
  lib,
  stdenv,
  src,
  cargo,
  cmake,
  fcitx5,
  gettext,
  hicolor-icon-theme,
  kdePackages,
  libxcb,
  librsvg,
  ninja,
  pkg-config,
  python3,
  qt6,
  rustc,
}:

let
  pythonEnv = python3.withPackages (ps: [
    ps.dbus-python
    ps.pyqt6
    ps.qtpy
  ]);

  cmakeLists = builtins.readFile ../CMakeLists.txt;
  version = builtins.head (
    builtins.match ".*project\\(fcitx5-lotus VERSION ([0-9.]+)\\).*" (
      builtins.replaceStrings [ "\n" ] [ " " ] cmakeLists
    )
  );
in
stdenv.mkDerivation {
  pname = "fcitx5-ngosen";
  inherit version src;

  nativeBuildInputs = [
    cargo
    cmake
    gettext
    kdePackages.extra-cmake-modules
    librsvg
    ninja
    pkg-config
    pythonEnv
    qt6.wrapQtAppsHook
    rustc
  ];

  buildInputs = [
    fcitx5
    kdePackages.extra-cmake-modules
    pythonEnv
    qt6.qtbase
    qt6.qtsvg
    qt6.qtwayland
  ];

  strictDeps = true;
  dontWrapQtApps = true;

  cmakeFlags = [
    (lib.cmakeBool "NGOSEN_RUST_CORE" true)
    (lib.cmakeBool "BUILD_TESTING" true)
    (lib.cmakeBool "LOTUS_BYTECOMPILE_PYTHON" false)
  ];

  doCheck = true;

  # The settings app looks for its translations and the system dictionary under /usr.
  postPatch = ''
    substituteInPlace settings-gui/i18n.py \
      --replace-fail '"/usr/share/locale"' "\"$out/share/locale\""
    substituteInPlace settings-gui/ui/pages/dict_editor.py \
      --replace-fail '"/usr/share/fcitx5/lotus/vietnamese.cm.dict"' \
                     "\"$out/share/fcitx5/lotus/vietnamese.cm.dict\""
  '';

  preConfigure = ''
    export CARGO_HOME=$TMPDIR/cargo
  '';

  # XTest replacement and the pointer watcher dlopen libxcb, so it has to be on the module's
  # runpath; patchelf --shrink-rpath would drop it as unused if it were added before fixup.
  postFixup = ''
    patchelf --add-rpath ${lib.makeLibraryPath [ libxcb ]} $out/lib/fcitx5/liblotus.so
    wrapQtApp $out/bin/fcitx5-lotus-settings \
      --prefix XDG_DATA_DIRS : "${hicolor-icon-theme}/share:$out/share"
  '';

  meta = {
    description = "Ngó Sen, a Vietnamese input method for Fcitx5";
    homepage = "https://github.com/ngosen/ngosen";
    license = with lib.licenses; [
      gpl3Plus
      lgpl21Plus
    ];
    platforms = lib.platforms.linux;
  };
}
