#!/bin/bash
# Rimuove Mecena Motion da /Library/OFX/Plugins e la ricevuta dell'installer.
echo "Rimozione di Mecena Motion (serve la password di amministratore)."
sudo rm -rf "/Library/OFX/Plugins/MecenaMotion.ofx.bundle"
sudo rm -f "/Library/Application Support/Blackmagic Design/DaVinci Resolve/Fusion/Scripts/Utility/Mecena ClipDB.py"
sudo rm -f /usr/local/bin/mecena-clipdb
sudo pkgutil --forget it.mecena.motion.pkg >/dev/null 2>&1
echo "Fatto. Riavvia DaVinci Resolve."
