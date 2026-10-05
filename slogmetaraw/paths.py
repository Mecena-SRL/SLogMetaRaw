# SPDX-License-Identifier: GPL-3.0-or-later
"""Where S-Log MetaRaw keeps its state on each system. The OpenFX plugin (src/common/Files.cpp) uses the
same folders: the cache written here is the one it reads.

  macOS    ~/Library/Application Support/SLogMetaRaw      logs in ~/Library/Logs/SLogMetaRaw
  Windows  %APPDATA%\\SLogMetaRaw                          logs in <support>\\logs
  Linux    $XDG_DATA_HOME or ~/.local/share/SLogMetaRaw   logs in <support>/logs
"""
import json
import os
import sys
import time


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


def write_json(path, data, tmp_tag=''):
    """Replace path with data in one step: the plugin never reads a half-written file.

    On Windows the replace fails while the plugin has the file open; it is retried for ~0.5 s.
    Raises OSError, leaving no temporary file behind.
    """
    tmp = '%s.%d%s.tmp' % (path, os.getpid(), tmp_tag)
    try:
        with open(tmp, 'w', encoding='utf-8') as fh:
            json.dump(data, fh, ensure_ascii=False)
        for attempt in range(10):
            try:
                os.replace(tmp, path)
                return
            except PermissionError:
                if os.name != 'nt' or attempt == 9:
                    raise
                time.sleep(0.05)
    except OSError:
        try:
            os.unlink(tmp)
        except OSError:
            pass
        raise
