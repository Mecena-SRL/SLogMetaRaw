# SPDX-License-Identifier: GPL-3.0-or-later
"""Connection to the local DaVinci Resolve scripting server and single-clip write.

Used by ``--to-resolve``, the child the plugin starts for "Rileggi metadata". Resolve 21.x can
refuse 127.0.0.1 while accepting an interface address, so the fallback tries the local IPv4s.
"""

import os
import socket
import sys
import subprocess
import time
import unicodedata


def _local_ipv4_addresses():
    """Addresses assigned to this computer only; never discover other Resolve hosts.

    macOS lists them with ifconfig, Linux with ip (ifconfig is often missing); Windows has neither, so it takes the
    address of the default route (a UDP connect sends nothing) and those of the host name.
    """
    candidates = []
    if sys.platform == 'win32':
        try:
            with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as s:
                s.connect(('192.0.2.1', 9))
                candidates.append(s.getsockname()[0])
        except OSError:
            pass
        try:
            candidates += socket.gethostbyname_ex(socket.gethostname())[2]
        except OSError:
            pass
    else:
        commands = [['/sbin/ifconfig', '-a']] if sys.platform == 'darwin' else [['ip', '-4', '-o', 'addr', 'show'],
                                                                                 ['/sbin/ifconfig', '-a']]
        for command in commands:
            try:
                listing = subprocess.run(command, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                                         universal_newlines=True, timeout=2, check=True).stdout
            except (OSError, subprocess.SubprocessError):
                continue
            for line in listing.splitlines():
                fields = line.split()
                # "inet 10.0.0.3 netmask" (ifconfig), "inet 10.0.0.3/24" (ip), "inet addr:10.0.0.3" (old net-tools)
                candidates += [fields[i + 1].split('/')[0].replace('addr:', '')
                               for i in range(len(fields) - 1) if fields[i] == 'inet']
            break
    addresses = []
    for address in candidates:
        try:
            socket.inet_pton(socket.AF_INET, address)
        except OSError:
            continue
        if address != '0.0.0.0' and not address.startswith('127.') and address not in addresses:
            addresses.append(address)
    return addresses


def scripting_module_dirs():
    """Folders where Resolve installs DaVinciResolveScript.py, for a python3 that Resolve did not start itself."""
    dirs = []
    api = os.environ.get('RESOLVE_SCRIPT_API')
    if api:
        dirs.append(os.path.join(api, 'Modules'))
    if sys.platform == 'darwin':
        dirs.append('/Library/Application Support/Blackmagic Design/DaVinci Resolve/Developer/Scripting/Modules')
    elif os.name == 'nt':
        dirs.append(os.path.join(os.environ.get('PROGRAMDATA', 'C:\\ProgramData'), 'Blackmagic Design', 'DaVinci Resolve',
                                 'Support', 'Developer', 'Scripting', 'Modules'))
    else:
        dirs.append('/opt/resolve/Developer/Scripting/Modules')
    return [d for d in dirs if os.path.isdir(d)]


def _resolve_script_module():
    """DaVinciResolveScript: Resolve's own python has it on its path, a system python3 (Linux, Windows) does not."""
    try:
        import DaVinciResolveScript
        return DaVinciResolveScript
    except ImportError:
        for folder in scripting_module_dirs():
            if folder not in sys.path:
                sys.path.append(folder)
        import DaVinciResolveScript
        return DaVinciResolveScript


def connect(timeout=None):
    """A connected Resolve object, or raise RuntimeError with a user-readable reason.

    With a timeout the fallback addresses share what is left of it, instead of 1 s each."""
    end = None if timeout is None else time.monotonic() + timeout
    try:
        bmd = _resolve_script_module()
    except Exception as exc:
        raise RuntimeError('modulo DaVinciResolveScript non disponibile (%s)' % exc)
    resolve = None
    try:
        resolve = bmd.scriptapp('Resolve')
    except Exception:
        resolve = None
    if resolve:
        return resolve
    for address in _local_ipv4_addresses():
        wait = 1.0 if end is None else min(1.0, end - time.monotonic())
        if wait <= 0.05:
            break
        try:
            resolve = bmd.scriptapp('Resolve', address, wait)
        except Exception:
            resolve = None
        if resolve:
            return resolve
    raise RuntimeError('connessione a DaVinci Resolve non riuscita: controlla che '
                       'Resolve sia aperto con un progetto e che lo scripting esterno sia attivo')


def _nfc(path):
    return unicodedata.normalize('NFC', path)


def apply_path(resolve, path, r, set_color_space=False, overwrite=True, add_tags=False,
               set_data_level=True):
    """Write the metadata of one clip into every Media Pool item with that file path.

    Same defaults as the script window (sovrascrivi attivo, tag spenti, input color
    space non toccato). Returns {'written': n, 'failed': n, 'clips': n} or raises
    RuntimeError when the clip is not in the Media Pool."""
    project = resolve.GetProjectManager().GetCurrentProject()
    if not project:
        raise RuntimeError('nessun progetto aperto in Resolve')
    pool = project.GetMediaPool()
    if not pool:
        raise RuntimeError('Media Pool non disponibile')

    from . import resolve_io
    pool_paths = [(clip, resolve_io.clip_path(clip))
                  for clip in resolve_io.iter_media_pool_clips(pool.GetRootFolder())]
    pool_paths = [(clip, _nfc(p)) for clip, p in pool_paths if p]
    wanted = _nfc(path)
    matches = [clip for clip, p in pool_paths if p == wanted]
    if not matches:
        # a symlink on either side: realpath touches the disk, so only when nothing matched
        real = _nfc(os.path.realpath(path))
        matches = [clip for clip, p in pool_paths if _nfc(os.path.realpath(p)) == real]
    written = failed = clips = 0
    level = {}
    for clip in matches:
        report = resolve_io.apply_to_clip(clip, r, set_color_space=set_color_space,
                                          overwrite=overwrite, add_tags=add_tags,
                                          set_data_level=set_data_level)
        written += len(report.get('written') or [])
        failed += len(report.get('failed') or [])
        level = report.get('data_level') or level
        clips += 1
    if clips == 0:
        raise RuntimeError('clip non trovata nel Media Pool: importa la clip e riprova')
    return {'written': written, 'failed': failed, 'clips': clips, 'data_level': level}
