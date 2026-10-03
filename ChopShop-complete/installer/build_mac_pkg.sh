#!/bin/bash
# macOS installer (.pkg) that auto-installs VST3 (+ AAX if built) into the system plugin folders.
set -e
cd "$(dirname "$0")/.."
ART=build/ChopShop_artefacts/Release
VER=1.0.0
# AAX_ONLY=1 ./installer/build_mac_pkg.sh  -> installer/ChopShop-AAX-<ver>.pkg (Pro Tools only)
rm -rf pkgroot && mkdir -p pkgroot
PKGS=()
OUT=installer/ChopShop-$VER.pkg
if [ -z "$AAX_ONLY" ]; then
  mkdir -p pkgroot/vst3/Library/Audio/Plug-Ins/VST3
  cp -R "$ART/VST3/ChopShop.vst3" pkgroot/vst3/Library/Audio/Plug-Ins/VST3/
  pkgbuild --root pkgroot/vst3 --identifier com.chopshop.vst3 --version $VER pkgroot/vst3.pkg
  PKGS+=(--package pkgroot/vst3.pkg)
else
  OUT=installer/ChopShop-AAX-$VER.pkg
fi
if [ -d "$ART/AAX/ChopShop.aaxplugin" ]; then
  mkdir -p "pkgroot/aax/Library/Application Support/Avid/Audio/Plug-Ins"
  cp -R "$ART/AAX/ChopShop.aaxplugin" "pkgroot/aax/Library/Application Support/Avid/Audio/Plug-Ins/"
  pkgbuild --root pkgroot/aax --identifier com.chopshop.aax --version $VER pkgroot/aax.pkg
  PKGS+=(--package pkgroot/aax.pkg)
fi
productbuild "${PKGS[@]}" "$OUT"
echo "Built $OUT"
# For distribution: codesign with a Developer ID Installer cert and notarize (xcrun notarytool).
