# SPDX-License-Identifier: GPL-3.0-or-later
"""S-Log MetaRaw launcher for DaVinci Resolve: Workspace > Scripts.

Installed by install.sh into Fusion/Scripts/Utility; LIB_DIR is replaced at
install time with the folder that contains the 'slogmetaraw' package. A copy left
as is (the Windows installer, which may find no Python to render it) reads that
folder from the lib_path file the installer writes next to the plugin's state.
Startup failures are reported in a dialog and in launcher.log (macOS ~/Library/Logs/SLogMetaRaw,
Windows %APPDATA%\\SLogMetaRaw\\logs, Linux ~/.local/share/SLogMetaRaw/logs).
"""
import os
import socket
import subprocess
import sys
import time
import traceback

LIB_DIR = '__LIB_DIR__'


def _log_path():
    # same folders as slogmetaraw/paths.py (this file runs before the package is importable)
    home = os.path.expanduser('~')
    if sys.platform == 'darwin':
        return os.path.join(home, 'Library', 'Logs', 'SLogMetaRaw', 'launcher.log')
    if os.name == 'nt':
        base = os.environ.get('APPDATA') or os.path.join(home, 'AppData', 'Roaming')
    else:
        base = os.environ.get('XDG_DATA_HOME') or os.path.join(home, '.local', 'share')
    return os.path.join(base, 'SLogMetaRaw', 'logs', 'launcher.log')


LOG_PATH = _log_path()
PROJECT_WAIT = 3.0   # seconds for Fusion and the open project once Resolve answers


def _log(message):
    """Diagnostics must never prevent the script from opening."""
    try:
        os.makedirs(os.path.dirname(LOG_PATH), exist_ok=True)
        with open(LOG_PATH, 'a', encoding='utf-8') as stream:
            stream.write('%s %s\n' % (time.strftime('%Y-%m-%d %H:%M:%S'), message))
    except OSError:
        pass


def _report_error(message, detail):
    _log(detail)
    try:
        print('S-Log MetaRaw: %s\n%s' % (message, detail), file=sys.stderr)
    except (OSError, AttributeError):
        pass
    _show_dialog(message[:700] + '\n\nDettagli: ' + LOG_PATH)


def _show_dialog(text):
    try:
        if sys.platform == 'darwin':
            # Passing the message as argv preserves accents and quotes without generating
            # AppleScript source from exception text or a user's installation path.
            script = ('on run argv\n'
                      'display dialog (item 1 of argv) with title "S-Log MetaRaw" '
                      'buttons {"OK"} default button "OK" with icon caution\n'
                      'end run')
            subprocess.run(['/usr/bin/osascript', '-e', script, text], timeout=30, check=False)
        elif os.name == 'nt':
            import ctypes
            ctypes.windll.user32.MessageBoxW(None, text, 'S-Log MetaRaw', 0x30)
        else:
            for tool in (['zenity', '--warning', '--no-markup', '--title=S-Log MetaRaw', '--text=' + text],
                         ['kdialog', '--title', 'S-Log MetaRaw', '--sorry', text]):
                try:
                    subprocess.run(tool, timeout=30, check=False)
                    return
                except FileNotFoundError:
                    continue
    except (OSError, AttributeError, subprocess.SubprocessError):
        pass


def _retry(obtain, message, attempts=40, interval=0.25):
    """Allow Resolve to finish connecting/loading, retaining the last failure."""
    last_error = None
    deadline = time.monotonic() + attempts * interval
    for attempt in range(attempts):
        try:
            value = obtain()
            if value is not None and value is not False:
                return value
        except Exception as exc:
            last_error = exc
        remaining = deadline - time.monotonic()
        if remaining <= 0:
            break
        if attempt + 1 < attempts:
            time.sleep(min(interval, remaining))
    raise RuntimeError(message) from last_error


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


def _connect(namespace, bmd):
    """Prefer the menu's existing connection before opening another API session.

    Depending on Resolve's launch context, Fusion is injected as fusion, fu or
    app. Its GetResolve() can supply the connection even when scriptapp('Resolve')
    is unavailable. A failure in one route must not skip the remaining routes.
    """
    routes = [('resolve globale', lambda: namespace.get('resolve'))]
    for name in ('fusion', 'fu', 'app'):
        obj = namespace.get(name)
        if obj is not None:
            routes.append((name + '.GetResolve()', lambda obj=obj: obj.GetResolve()))
    routes.extend([
        ("scriptapp('Resolve')", lambda: bmd.scriptapp('Resolve')),
        ("scriptapp('Fusion').GetResolve()", lambda: bmd.scriptapp('Fusion').GetResolve()),
    ])
    last_error = None
    for label, obtain in routes:
        try:
            resolve = obtain()
            if resolve is not None and resolve is not False:
                _log('Connessione: ' + label)
                return resolve
        except Exception as exc:
            last_error = exc
    for address in _local_ipv4_addresses():
        try:
            resolve = bmd.scriptapp('Resolve', address, 1.0)
            if resolve is not None and resolve is not False:
                _log('Connessione: indirizzo locale ' + address)
                return resolve
        except Exception as exc:
            last_error = exc
    if last_error is not None:
        raise last_error
    return None


def _fusion(namespace, resolve):
    for name in ('fusion', 'fu', 'app'):
        candidate = namespace.get(name)
        if candidate is not None and getattr(candidate, 'UIManager', None) is not None:
            return candidate
    return resolve.Fusion()


def _support_dir():
    # same folder as slogmetaraw/paths.support_dir()
    home = os.path.expanduser('~')
    if sys.platform == 'darwin':
        return os.path.join(home, 'Library', 'Application Support', 'SLogMetaRaw')
    if os.name == 'nt':
        return os.path.join(os.environ.get('APPDATA') or os.path.join(home, 'AppData', 'Roaming'), 'SLogMetaRaw')
    return os.path.join(os.environ.get('XDG_DATA_HOME') or os.path.join(home, '.local', 'share'), 'SLogMetaRaw')


def _lib_dir():
    """The library folder: the rendered LIB_DIR, else the one recorded in <support>/lib_path."""
    if not LIB_DIR.startswith('__'):
        return LIB_DIR
    try:
        with open(os.path.join(_support_dir(), 'lib_path'), encoding='utf-8-sig') as stream:
            recorded = stream.read().strip()
    except (OSError, ValueError):   # missing, or not UTF-8: the installers' default folder
        recorded = ''
    return recorded or os.path.join(_support_dir(), 'lib')


def _load_ui():
    lib_dir = _lib_dir()
    if not os.path.isfile(os.path.join(lib_dir, 'slogmetaraw', '__init__.py')):
        raise RuntimeError('La libreria S-Log MetaRaw non si trova in:\n%s\n'
                           'Reinstalla lo script dalla cartella del programma.' % lib_dir)
    if lib_dir in sys.path:
        sys.path.remove(lib_dir)
    sys.path.insert(0, lib_dir)
    for name in list(sys.modules):
        if name == 'slogmetaraw' or name.startswith('slogmetaraw.'):
            del sys.modules[name]  # always pick up the installed version
    from slogmetaraw import ui
    return ui


def main(namespace=None):
    namespace = globals() if namespace is None else namespace
    _log('Avvio: Python %s; eseguibile=%s; libreria=%s; globals=%s' % (
        sys.version.split()[0], sys.executable, _lib_dir(),
        ', '.join(name for name in ('bmd', 'resolve', 'fusion', 'fu', 'app')
                  if namespace.get(name) is not None)))
    try:
        bmd = namespace.get('bmd')
        if bmd is None:
            import DaVinciResolveScript as bmd
        resolve = namespace.get('resolve')
        if resolve is not None:
            _log('Connessione: resolve globale')
        else:
            # started outside the menu: Resolve may still be finishing its launch
            resolve = _retry(lambda: _connect(namespace, bmd),
                             'S-Log MetaRaw non riesce a collegarsi a DaVinci Resolve.\n'
                             'Apri un progetto e rilancia da Workspace > Scripts. '
                             'Se il problema persiste, chiudi e riapri Resolve.')
        fusion = _retry(lambda: _fusion(namespace, resolve),
                        'L\'interfaccia Fusion di DaVinci Resolve non è disponibile.',
                        attempts=int(PROJECT_WAIT / 0.25))
        _retry(lambda: resolve.GetProjectManager().GetCurrentProject(),
               'Non c\'è un progetto aperto. Apri un progetto in DaVinci Resolve '
               'e rilancia S-Log MetaRaw da Workspace > Scripts.',
               attempts=int(PROJECT_WAIT / 0.25))
        if getattr(fusion, 'UIManager', None) is None:
            raise RuntimeError('UIManager non è disponibile. La finestra S-Log MetaRaw '
                               'richiede DaVinci Resolve Studio.')
        ui = _load_ui()
        _log('Avvio interfaccia')
        ui.main(resolve, fusion, bmd)
        _log('Interfaccia chiusa')
        return True
    except Exception as exc:
        _report_error(str(exc), traceback.format_exc())
        return False


if globals().get('__name__', '__main__') == '__main__':
    main()
