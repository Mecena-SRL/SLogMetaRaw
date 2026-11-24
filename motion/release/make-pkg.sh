#!/bin/bash
# Crea MecenaMotion-<versione>.pkg che installa il bundle in /Library/OFX/Plugins.
# Uso: release/make-pkg.sh <MecenaMotion.ofx.bundle> <versione> <cartella di uscita>
# Firma facoltativa: MECENA_INSTALLER_SIGN="Developer ID Installer: …"
# Notarizzazione facoltativa: MECENA_NOTARY_PROFILE=<profilo di xcrun notarytool store-credentials>
set -euo pipefail
bundle=$1 version=$2 out=$3
here="$(cd "$(dirname "$0")" && pwd)"
[[ -d "$bundle" ]] || { echo "Bundle mancante: $bundle"; exit 1; }
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
mkdir -p "$work/root/Library/OFX/Plugins" "$work/scripts" "$work/res" "$out"
ditto "$bundle" "$work/root/Library/OFX/Plugins/MecenaMotion.ofx.bundle"
install -m 755 "$here/installer/preinstall" "$here/installer/postinstall" "$work/scripts/"
cp "$here/installer/"*.html "$work/res/"
cp "$here/../LICENSE" "$work/res/LICENSE.txt"
# Il bundle va sempre in /Library/OFX/Plugins, anche se esiste una copia altrove.
pkgbuild --analyze --root "$work/root" "$work/components.plist"
pb=/usr/libexec/PlistBuddy
i=0
while $pb -c "Print :$i" "$work/components.plist" >/dev/null 2>&1; do
    $pb -c "Delete :$i:BundleIsRelocatable" "$work/components.plist" >/dev/null 2>&1 || true
    $pb -c "Add :$i:BundleIsRelocatable bool false" "$work/components.plist"
    i=$((i+1))
done
pkgbuild --root "$work/root" --install-location / --component-plist "$work/components.plist" \
    --scripts "$work/scripts" --identifier it.mecena.motion.pkg --version "$version" \
    "$work/MecenaMotion-core.pkg"
sed -e "s/@VERSION@/$version/g" -e "s/@ARCH@/$(uname -m)/g" "$here/installer/distribution.xml" > "$work/distribution.xml"
pkg="$out/MecenaMotion-$version.pkg"
sign=()
[[ -n "${MECENA_INSTALLER_SIGN:-}" ]] && sign=(--sign "$MECENA_INSTALLER_SIGN" --timestamp)
productbuild --distribution "$work/distribution.xml" --package-path "$work" \
    --resources "$work/res" ${sign[@]+"${sign[@]}"} "$pkg"
if [[ -n "${MECENA_NOTARY_PROFILE:-}" && ${#sign[@]} -gt 0 ]]; then
    xcrun notarytool submit "$pkg" --keychain-profile "$MECENA_NOTARY_PROFILE" --wait
    xcrun stapler staple "$pkg"
fi
pkgutil --check-signature "$pkg" || true
echo "Installer: $pkg"
