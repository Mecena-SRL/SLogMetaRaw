#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Mecena ClipDB per DaVinci Resolve.

Dalla timeline corrente:
  1. raccoglie gli originali delle clip video (anche se in timeline si lavora con i proxy);
  2. aggiorna il database MecenaClipDB con mecena-clipdb (solo le clip nuove o cambiate);
  3. scrive la mappa timeline -> clip in MecenaClipDB/timelines/ e la rende attiva.

Con la mappa attiva, Mecena Stabilize su un adjustment clip stabilizza ogni frame con i dati giroscopio, accelerometro e
ottica della clip che si trova sotto. Su una clip normale il database basta anche senza mappa.

Installazione: copiare in
  ~/Library/Application Support/Blackmagic Design/DaVinci Resolve/Fusion/Scripts/Utility/
e avviare da Workspace > Scripts > Utility > Mecena ClipDB.

Variabili facoltative: MECENA_CLIPDB (cartella del database), MECENA_CLIPDB_BIN (percorso di mecena-clipdb).
"""
import json
import os
import re
import subprocess
import sys

BUNDLE_BIN = "/Library/OFX/Plugins/MecenaMotion.ofx.bundle/Contents/Resources/mecena-clipdb"
FORMAT = "mecena-clipdb-timeline"


def config_path():
    home = os.path.expanduser("~")
    if sys.platform == "darwin":
        return os.path.join(home, "Library/Application Support/Mecena/clipdb.json")
    return os.path.join(home, ".config/mecena/clipdb.json")


def read_config():
    try:
        with open(config_path()) as f:
            return json.load(f)
    except (OSError, ValueError):
        return {}


def write_config(cfg):
    os.makedirs(os.path.dirname(config_path()), exist_ok=True)
    with open(config_path(), "w") as f:
        json.dump(cfg, f, indent=2)


def get_resolve():
    # Resolve's script menu provides `resolve` (and Fusion's `app`) as globals
    if globals().get("resolve"):
        return globals()["resolve"]
    if globals().get("app"):
        return globals()["app"].GetResolve()
    try:
        import DaVinciResolveScript as dvr
        return dvr.scriptapp("Resolve")
    except ImportError:
        return None


def number(v, default=0.0):
    try:
        return float(v)
    except (TypeError, ValueError):
        return default


def item_source_range(item, clip_fps, timeline_fps, duration):
    """Primo frame dell'originale e frame dell'originale per frame di timeline."""
    start = end = None
    if hasattr(item, "GetSourceStartFrame"):
        start = item.GetSourceStartFrame()
        end = item.GetSourceEndFrame() if hasattr(item, "GetSourceEndFrame") else None
    if start is None:
        start = item.GetLeftOffset() or 0
    start = number(start)
    rate = 0.0
    if end is not None and duration > 0:
        rate = (number(end) - start) / duration
    if rate <= 0:
        # Senza retime Resolve riproduce a velocità reale: 50p su 25p avanza di due frame dell'originale
        rate = clip_fps / timeline_fps if clip_fps > 0 and timeline_fps > 0 else 1.0
    return start, rate


def tc_seconds(tc, fps):
    """Secondi dalla mezzanotte di un timecode HH:MM:SS:FF (o ;FF drop-frame) come lo mostra Resolve."""
    m = re.match(r"^(\d+):(\d+):(\d+)[:;.](\d+)$", (tc or "").strip())
    if not m or fps <= 0:
        return None
    h, mi, se, fr = (int(x) for x in m.groups())
    base = int(round(fps))
    frames = ((h * 60 + mi) * 60 + se) * base + fr
    if ";" in tc and base in (30, 60):
        drop = 2 * base // 30
        minutes = h * 60 + mi
        frames -= drop * (minutes - minutes // 10)
        return frames * 1001.0 / (base * 1000.0)
    return frames / float(base)


def pool_clips(folder):
    for clip in folder.GetClipList() or []:
        yield clip
    for sub in folder.GetSubFolderList() or []:
        for clip in pool_clips(sub):
            yield clip


def load_index(db):
    try:
        with open(os.path.join(db, "index.json")) as f:
            return json.load(f)
    except (OSError, ValueError):
        return None


def save_index(db, index):
    tmp = os.path.join(db, "index.json.tmp")
    with open(tmp, "w") as f:
        json.dump(index, f, indent=2)
    os.replace(tmp, os.path.join(db, "index.json"))


def clip_info(clip):
    path = clip.GetClipProperty("File Path") or ""
    fps = number(clip.GetClipProperty("FPS"))
    start = tc_seconds(clip.GetClipProperty("Start TC"), fps)
    frames = number(clip.GetClipProperty("Frames"))
    if not path or start is None or fps <= 0 or frames <= 0:
        return None
    return {"path": path, "name": os.path.basename(path), "fps": fps, "start": start, "end": start + frames / fps,
            "tc": clip.GetClipProperty("Start TC")}


def link_external_recordings(db, project):
    """Collega ai dati della clip interna i file senza dati movimento propri (ProRes RAW e altri registratori esterni).

    L'abbinamento usa il timecode: la camera lo manda al registratore via HDMI/SDI. Vince la clip interna che copre la
    parte più lunga del file esterno, purché ne copra almeno metà. Lo scarto fra i due inizi diventa l'offset in frame.
    I collegamenti manuali (mecena-clipdb link) restano come sono.
    """
    index = load_index(db)
    if not index:
        return 0
    infos = [i for i in (clip_info(c) for c in pool_clips(project.GetMediaPool().GetRootFolder())) if i]
    by_path = {c["path"]: c for c in index.get("clips", [])}
    internals = []
    for info in infos:
        entry = by_path.get(info["path"])
        if entry:
            if info["tc"] and entry.get("start_tc") != info["tc"]:
                entry["start_tc"] = info["tc"]
            internals.append((info, entry))
    links = index.setdefault("links", [])
    manual = {l["external"] for l in links if l.get("method") == "manuale"}
    made = 0
    for ext in infos:
        if ext["path"] in by_path or ext["path"] in manual:
            continue
        best, best_overlap = None, 0.0
        for info, entry in internals:
            overlap = min(ext["end"], info["end"]) - max(ext["start"], info["start"])
            if overlap > best_overlap:
                best, best_overlap = (info, entry), overlap
        if not best or best_overlap < 0.5 * (ext["end"] - ext["start"]):
            continue
        info, entry = best
        offset = round((ext["start"] - info["start"]) * info["fps"], 3)
        links[:] = [l for l in links if l.get("external") != ext["path"]]
        links.append({"external": ext["path"], "clip_id": entry["id"], "frame_offset": offset, "method": "timecode",
                      "note": "TC %s su %s" % (ext["tc"], info["tc"])})
        made += 1
        print("Collegato %s → %s (offset %+g frame)" % (ext["name"], info["name"], offset))
    save_index(db, index)
    return made


def main():
    resolve = get_resolve()
    if not resolve:
        print("Resolve non raggiungibile: avviare lo script da Workspace > Scripts.")
        return 1
    project = resolve.GetProjectManager().GetCurrentProject()
    timeline = project and project.GetCurrentTimeline()
    if not timeline:
        print("Nessuna timeline aperta.")
        return 1

    cfg = read_config()
    db = os.environ.get("MECENA_CLIPDB") or (cfg.get("databases") or [None])[0] or os.path.expanduser("~/Movies/MecenaClipDB")
    tool = os.environ.get("MECENA_CLIPDB_BIN") or BUNDLE_BIN
    tl_fps = number(timeline.GetSetting("timelineFrameRate"), 25.0)
    tl_start = int(timeline.GetStartFrame())

    segments, adjustments, media = [], [], []
    for track in range(1, int(timeline.GetTrackCount("video")) + 1):
        for item in timeline.GetItemListInTrack("video", track) or []:
            start = int(item.GetStart())
            duration = int(item.GetDuration())
            mpi = item.GetMediaPoolItem()
            path = mpi.GetClipProperty("File Path") if mpi else ""
            kind = (mpi.GetClipProperty("Type") if mpi else "") or ""
            if not path:
                if "adjust" in (item.GetName() or "").lower() or "adjust" in kind.lower():
                    adjustments.append({"start": start, "end": start + duration, "track": track})
                continue
            clip_fps = number(mpi.GetClipProperty("FPS"), tl_fps)
            source_start, rate = item_source_range(item, clip_fps, tl_fps, duration)
            segments.append({"start": start, "end": start + duration, "track": track, "media": path,
                             "source_start": source_start, "source_rate": rate})
            if path not in media:
                media.append(path)

    # Anche le clip del Media Pool fuori timeline: le interne registrate insieme a un RAW esterno
    for clip in pool_clips(project.GetMediaPool().GetRootFolder()):
        path = clip.GetClipProperty("File Path") or ""
        if path and path not in media:
            media.append(path)
    online = [m for m in media if os.path.isfile(m)]
    if online and os.path.isfile(tool):
        print("Aggiorno %s con %d originali…" % (db, len(online)))
        subprocess.call([tool, "scan", db] + online)
    elif online:
        print("mecena-clipdb non trovato in %s: la mappa viene scritta, il database non si aggiorna." % tool)
    offline = len(media) - len(online)
    if offline:
        print("%d originali offline: servono già nel database (scansione delle schede all'ingest)." % offline)

    link_external_recordings(db, project)

    name = re.sub(r"[^\w.-]+", "_", "%s__%s" % (project.GetName(), timeline.GetName()))
    out_dir = os.path.join(db, "timelines")
    os.makedirs(out_dir, exist_ok=True)
    out = os.path.join(out_dir, name + ".json")
    data = {"format": FORMAT, "version": 1, "timeline": timeline.GetName(), "fps": tl_fps, "start_frame": tl_start,
            "db": "", "adjustments": adjustments, "segments": segments}
    with open(out + ".tmp", "w") as f:
        json.dump(data, f, indent=1)
    os.replace(out + ".tmp", out)

    cfg = read_config()
    cfg["active_timeline"] = os.path.abspath(out)
    dbs = cfg.setdefault("databases", [])
    if os.path.abspath(db) not in dbs:
        dbs.append(os.path.abspath(db))
    write_config(cfg)
    print("Mappa attiva: %s · %d clip, %d adjustment clip" % (out, len(segments), len(adjustments)))
    if len(adjustments) > 1:
        print("Più adjustment clip: Stabilize li distingue dalla durata; con durate uguali usa il primo.")
    return 0


if __name__ == "__main__":
    main()
