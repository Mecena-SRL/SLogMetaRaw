# SPDX-License-Identifier: GPL-3.0-or-later
"""Paths of the built plugin and its test binaries; `make test-bins` compiles them with the shipped flags."""
import glob
import os
import shutil
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OFX = os.path.join(ROOT, 'ofx', 'SLogMetaRaw')
MACOS = sys.platform == 'darwin'
CMAKE_BUILD = os.path.join(OFX, 'build')
if MACOS:
    BUNDLE_BINARY = os.path.join(OFX, 'SLogMetaRaw.ofx.bundle', 'Contents', 'MacOS', 'SLogMetaRaw.ofx')
else:
    BUNDLE_BINARY = os.path.join(CMAKE_BUILD, 'SLogMetaRaw.ofx.bundle', 'Contents',
                                 'Win64' if os.name == 'nt' else 'Linux-x86-64', 'SLogMetaRaw.ofx')
# the plugin's state folder relative to $HOME (slogmetaraw/paths.py); tests run with HOME in a temp dir
if MACOS:
    SUPPORT_REL = 'Library/Application Support/SLogMetaRaw'
elif os.name == 'nt':
    SUPPORT_REL = 'AppData/Roaming/SLogMetaRaw'
else:
    SUPPORT_REL = '.local/share/SLogMetaRaw'
_built = None


def _build():
    """`make` on macOS; CMake elsewhere (OFX_SDK_DIR may point to a local OpenFX SDK, else it is fetched)."""
    if MACOS:
        return bool(shutil.which('make') and shutil.which('clang++')) and subprocess.run(
            ['make', '-s', '-C', OFX, 'all', 'test-bins'], capture_output=True).returncode == 0
    if not shutil.which('cmake'):
        return False
    cfg = ['cmake', '-S', OFX, '-B', CMAKE_BUILD, '-DCMAKE_BUILD_TYPE=Release']
    if os.environ.get('OFX_SDK_DIR'):
        cfg.append('-DOFX_SDK_DIR=' + os.environ['OFX_SDK_DIR'])
    return (subprocess.run(cfg, capture_output=True).returncode == 0 and subprocess.run(
        ['cmake', '--build', CMAKE_BUILD, '--config', 'Release', '--parallel'], capture_output=True).returncode == 0)


def test_bin(name):
    """Path of build/tests/<name>, after one `make test-bins` per test run; None when it cannot be built."""
    global _built
    if _built is None:
        _built = _build()
    path = os.path.join(CMAKE_BUILD, 'tests', name)
    if os.name == 'nt' and not path.endswith('.exe'):
        path += '.exe'
    return path if _built and os.path.exists(path) else None


def rosetta():
    """True when the x86_64 slice of a universal binary can run here."""
    return MACOS and subprocess.run(['arch', '-x86_64', '/usr/bin/true'], capture_output=True).returncode == 0


def source():
    """All plugin sources (src/**/*.h, *.cpp) concatenated, for the checks that read the code."""
    parts = []
    for path in sorted(glob.glob(os.path.join(OFX, 'src', '**', '*.*'), recursive=True)):
        if path.endswith(('.h', '.cpp')):
            with open(path, encoding='utf-8') as fh:
                parts.append(fh.read())
    return '\n'.join(parts)


def body(src, signature):
    """Text of the function whose definition starts with `signature`, up to its closing brace."""
    start = src.index(signature)
    return src[start:src.index('\n}\n', start) + 2]
