# SPDX-License-Identifier: GPL-3.0-or-later
"""Where S-Log MetaRaw keeps its state on each system. The OpenFX plugin (src/common/Files.cpp) uses the
same folders: the cache written here is the one it reads.

  macOS    ~/Library/Application Support/SLogMetaRaw      logs in ~/Library/Logs/SLogMetaRaw
  Windows  %APPDATA%\\SLogMetaRaw                          logs in <support>\\logs
  Linux    $XDG_DATA_HOME or ~/.local/share/SLogMetaRaw   logs in <support>/logs
"""
import os
import sys


def support_dir():
    home = os.path.expanduser('~')
    if sys.platform == 'darwin':
        return os.path.join(home, 'Library', 'Application Support', 'SLogMetaRaw')
    if os.name == 'nt':
        return os.path.join(os.environ.get('APPDATA') or os.path.join(home, 'AppData', 'Roaming'), 'SLogMetaRaw')
    return os.path.join(os.environ.get('XDG_DATA_HOME') or os.path.join(home, '.local', 'share'), 'SLogMetaRaw')


def log_dir():
    if sys.platform == 'darwin':
        return os.path.join(os.path.expanduser('~'), 'Library', 'Logs', 'SLogMetaRaw')
    return os.path.join(support_dir(), 'logs')
