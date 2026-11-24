# SPDX-License-Identifier: GPL-3.0-or-later
# Log MetaRaw - Copyright (C) 2026 Ivan Mazzone
# Free software under the GNU General Public License v3 or later; no warranty.
# See the LICENSE file or https://www.gnu.org/licenses/gpl-3.0.html
"""Log MetaRaw: read camera log footage metadata (Sony XAVC MP4/MXF and others) for DaVinci Resolve.

Ivan Mazzone + Claude (@Ivan_94m).
"""
from .extract import read_clip, DatalessError, find_sidecar  # noqa: F401

__version__ = '3.2.2'
