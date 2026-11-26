#!/bin/bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Removes Log MetaRaw Audio (VST3 and Audio Unit) and its installer receipts. The rest of Log MetaRaw, the DAW
# projects and their automation are not touched.
#   --dry-run   list what would be removed and remove nothing
# Tests only: SLOGMETARAW_ROOT (prefix of the system paths), SLOGMETARAW_YES=1, SLOGMETARAW_NO_SUDO=1.
ROOT="${SLOGMETARAW_ROOT:-}"
YES="${SLOGMETARAW_YES:-0}"
SUDO=sudo
[[ "${SLOGMETARAW_NO_SUDO:-0}" == 1 ]] && SUDO=
DRY=0
[[ "${1:-}" == "--dry-run" ]] && DRY=1
SYSTEM=(
  "$ROOT/Library/Audio/Plug-Ins/VST3/Log MetaRaw Audio.vst3"
  "$ROOT/Library/Audio/Plug-Ins/Components/Log MetaRaw Ripresa.component"
  "$ROOT/Library/Audio/Plug-Ins/Components/Log MetaRaw Mix e supporto.component"
)
RECEIPTS=(com.slogmetaraw.audio.pkg com.slogmetaraw.audiounit.pkg)
exists() { [[ -e "$1" || -L "$1" ]]; }
finish() {
  [[ -t 0 && "$YES" != 1 ]] && read -r -p "Premi Invio per chiudere questa finestra. " _
  exit "$1"
}
echo "Disinstallazione di Log MetaRaw Audio (VST3 e Audio Unit)"
echo
found=() rec=()
for p in "${SYSTEM[@]}"; do exists "$p" && found+=("$p"); done
if [[ -z "$ROOT" ]]; then
  for r in "${RECEIPTS[@]}"; do pkgutil --pkg-info "$r" >/dev/null 2>&1 && rec+=("$r"); done
fi
if (( ${#found[@]} + ${#rec[@]} == 0 )); then
  echo "Non c'è niente da rimuovere: Log MetaRaw Audio non risulta installato."
  finish 0
fi
echo "Verrà rimosso:"
for p in "${found[@]}"; do echo "  $p"; done
for r in "${rec[@]}"; do echo "  ricevuta dell'installer $r"; done
echo
if [[ "$DRY" == 1 ]]; then echo "(Prova: non è stato rimosso niente.)"; finish 0; fi
if [[ "$YES" != 1 ]]; then
  read -r -p "Continuare? [s/N] " a
  [[ "$a" == "s" || "$a" == "S" ]] || { echo "Annullato: non è stato rimosso niente."; finish 0; }
fi
if [[ -n "$SUDO" ]]; then
  echo "Serve la password di amministratore."
  $SUDO -v || { echo "Password non accettata: non è stato rimosso niente."; finish 1; }
fi
for p in "${found[@]}"; do $SUDO rm -rf -- "$p"; done
for r in "${rec[@]}"; do $SUDO pkgutil --forget "$r" >/dev/null 2>&1; done
left=()
for p in "${found[@]}"; do exists "$p" && left+=("$p"); done
if (( ${#left[@]} > 0 )); then
  echo "Non sono riuscito a rimuovere:"; for p in "${left[@]}"; do echo "  $p"; done; finish 1
fi
echo "Log MetaRaw Audio è stato rimosso. Riapri la DAW: la lista dei plugin si aggiorna da sola."
finish 0
