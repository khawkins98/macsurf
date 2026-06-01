#!/usr/bin/env python3
"""manifest-to-mcpxml.py — convert the hand-maintained MacSurf.mcp manifest into a
genuine CodeWarrior Pro 8 (IDE 5.0) importable project XML.

Why: the repo's MacSurf.mcp mimics CW's export style but doesn't conform to the
real schema (wrong FILEFLAGS vocabulary, missing PATHFORMAT/<?codewarrior?> PI,
invented flat access-path form), so CW8's File->Import Project rejects it. This
converter restructures it against the schema captured from genuine CW exports
(see tools/codewarrior/reference/python-cw7-reference.xml and docs/CW-XML-SCHEMA notes).

Key choices:
- FILE/FILEREF entries use PATHTYPE Absolute + MacOS colon paths rooted at the
  staged guest layout (default: Back40:Desktop Folder:patrick:macsurf-source
  Folder:browser:...). Why not the portable PATHTYPE Name? The manifest has 28
  duplicate basenames (utils.c x3 etc.) so name resolution can't disambiguate.
  Library entries (MSL/CarbonLib) DO use Name (found via SystemSearchPaths).
- Scalar settings pass through (the manifest already uses real CW setting names);
  the invented flat UserSearchPath/SystemSearchPath entries are rebuilt as the
  genuine nested UserSearchPaths/SystemSearchPaths structures.
- FILEFLAGS 'compile' -> empty; 'link' -> empty (membership in FILELIST/LINKORDER
  is what makes a file build; there is no 'compile' flag in CW's vocabulary).
- Prologue (<?codewarrior?> PI) + full DOCTYPE copied from the genuine reference.

Usage:
  manifest-to-mcpxml.py [--manifest PATH] [--reference PATH] [--out PATH]
                        [--prefix MAC_PATH] [--ideversion 5.0] [--lf]

  --prefix   absolute MacOS path of the folder that CONTAINS the .mcp in the
             guest tree (default matches stage-on-bootvol.sh's layout)
  --lf       emit LF line endings instead of CR (CR is the default; CW is CR-native)
"""
import argparse
import os
import posixpath
import re
import sys
import xml.etree.ElementTree as ET
from xml.sax.saxutils import escape

DEFAULT_MANIFEST = os.path.join(os.path.dirname(__file__), "..", "..",
                                "browser", "netsurf", "frontends", "macos9", "MacSurf.mcp")
DEFAULT_REFERENCE = os.path.join(os.path.dirname(__file__), "reference", "python-cw7-reference.xml")
# Where the .mcp lives inside the guest source tree (stage-on-bootvol.sh layout).
DEFAULT_PREFIX = "Back40:Desktop Folder:patrick:macsurf-source Folder:browser:netsurf:frontends:macos9"
# The .mcp's location relative to the browser/ tree root, used to resolve ../ paths.
MCP_TREE_LOCATION = "browser/netsurf/frontends/macos9"

# Files renamed/deleted in the repo AFTER the manifest was last updated (the
# duplicate-basename rename sweeps: a2f5656d, dc62be41, fixes50b, fixes61...).
# Maps manifest path -> current repo-relative path, or None = file was deleted
# from the repo (drop it from the generated project).
MANIFEST_RENAMES = {
    "../../../utils/file.c": "browser/netsurf/utils/ns_file.c",
    "../../../utils/time.c": "browser/netsurf/utils/ns_time.c",
    "../../../utils/nsurl.c": "browser/netsurf/utils/nsurl/nsurl.c",
    "../../../utils/utf8.c": "browser/netsurf/utils/ns_utf8.c",
    "../../../utils/hashtable.c": "browser/netsurf/utils/ns_hashtable.c",
    "../../../content/handlers/html/css.c": "browser/netsurf/content/handlers/html/html_css.c",
    "font.c": "browser/netsurf/frontends/macos9/macos9_font.c",
    "macsurf_render_test.c": None,  # diagnostic, deleted in f3c16199
    "../../../../libparserutils/src/utils/errors.c": "browser/libparserutils/src/utils/pu_errors.c",
    "../../../../libparserutils/src/charset/encodings/utf8.c": "browser/libparserutils/src/charset/encodings/pu_utf8.c",
    "../../../../libhubbub/src/utils/errors.c": "browser/libhubbub/src/utils/hub_errors.c",
    "../../../../libhubbub/src/utils/string.c": "browser/libhubbub/src/utils/hub_string.c",
    "../../../../libhubbub/src/charset/detect.c": "browser/libhubbub/src/charset/libhubbub_detect.c",
    "../../../../libdom/src/utils/hashtable.c": "browser/libdom/src/utils/dom_hashtable.c",
    "../../../../libdom/src/core/string.c": "browser/libdom/src/core/dom_string.c",
    "../../../../libcss/src/utils/errors.c": "browser/libcss/src/utils/css_errors.c",
    "../../../../libcss/src/utils/utils.c": "browser/libcss/src/utils/css_utils.c",
    "../../../../libcss/src/charset/detect.c": "browser/libcss/src/charset/libcss_detect.c",
    "../../../../libcss/src/parse/parse.c": "browser/libcss/src/parse/css_parse.c",
    "../../../../libcss/src/parse/properties/content.c": "browser/libcss/src/parse/properties/p_content.c",
    "../../../../libcss/src/parse/properties/font.c": "browser/libcss/src/parse/properties/p_font.c",
    "../../../../libcss/src/parse/properties/utils.c": "browser/libcss/src/parse/properties/p_utils.c",
    "../../../../libcss/src/parse/properties/voice_family.c": "browser/libcss/src/parse/properties/p_voice_family.c",
    "../../../../libcss/src/select/properties/voice_family.c": "browser/libcss/src/select/properties/s_voice_family.c",
    "../../../../libcss/src/select/select.c": "browser/libcss/src/select/css_select.c",
}


def unix_rel_to_mac_abs(rel_path, prefix):
    """Resolve a manifest-relative unix path against the .mcp tree location and
    return an absolute MacOS colon path under `prefix`'s volume root.

    NB the manifest's `../` paths are FILE-relative (counted from MacSurf.mcp
    itself: `../../../utils/...` = browser/netsurf/utils/...), while bare paths
    (`shims/...`) are folder-relative. Verified against the repo tree + the
    manifest's own group comments. So strip ONE `../` level before folder-
    relative resolution."""
    # prefix = Back40:...:browser:netsurf:frontends:macos9  (colon path, no trailing colon)
    adjusted = rel_path
    if adjusted.startswith("../"):
        adjusted = adjusted[3:]
    joined = posixpath.normpath(posixpath.join(MCP_TREE_LOCATION, adjusted))
    # joined is now relative to the repo root (browser/...)
    if joined.startswith(".."):
        raise ValueError("path escapes the source tree: %s" % rel_path)
    # prefix's tail is .../browser:netsurf:frontends:macos9 ; we need the part of
    # prefix that corresponds to the parent of "browser":
    prefix_parts = prefix.split(":")
    tree_parts = MCP_TREE_LOCATION.split("/")
    assert prefix_parts[-len(tree_parts):] == tree_parts, \
        "--prefix must end with %s (got %s)" % (":".join(tree_parts), prefix)
    source_root = prefix_parts[:-len(tree_parts)]  # up to macsurf-source Folder
    return ":".join(source_root + joined.split("/")), joined


def settings_scalar(name, value):
    return '<SETTING><NAME>%s</NAME><VALUE>%s</VALUE></SETTING>' % (escape(name), escape(value or ""))


def search_path_block(path, path_format, path_root, recursive=False):
    return (
        "<SETTING>"
        "<SETTING><NAME>SearchPath</NAME>"
        "<SETTING><NAME>Path</NAME><VALUE>%s</VALUE></SETTING>"
        "<SETTING><NAME>PathFormat</NAME><VALUE>%s</VALUE></SETTING>"
        "<SETTING><NAME>PathRoot</NAME><VALUE>%s</VALUE></SETTING>"
        "</SETTING>"
        "<SETTING><NAME>Recursive</NAME><VALUE>%s</VALUE></SETTING>"
        "<SETTING><NAME>FrameworkPath</NAME><VALUE>false</VALUE></SETTING>"
        "<SETTING><NAME>HostFlags</NAME><VALUE>All</VALUE></SETTING>"
        "</SETTING>"
    ) % (escape(path), path_format, path_root, "true" if recursive else "false")


def convert_access_path(value, prefix):
    """Convert a manifest access-path value to (mac_path, path_root).

    {Project}/... paths are resolved to ABSOLUTE Back40 paths (using the same
    file-relative ../ convention as the FILE entries) so the imported .mcp works
    regardless of where it is saved. {Compiler}/... stays CodeWarrior-rooted
    (relative to the CW installation, location-independent by nature)."""
    if value.startswith("{Project}"):
        rest = value[len("{Project}"):].lstrip("/")
        if not rest:
            # the project folder itself
            return (prefix + ":", "Absolute")
        mac_path, _repo = unix_rel_to_mac_abs(rest, prefix)
        if not mac_path.endswith(":"):
            mac_path += ":"
        return (mac_path, "Absolute")
    elif value.startswith("{Compiler}"):
        rest = value[len("{Compiler}"):].lstrip("/")
        mac = ":" + rest.replace("/", ":")
        if not mac.endswith(":"):
            mac += ":"
        return (mac, "CodeWarrior")
    else:
        return (value, "Absolute")


def file_entry(pathtype, path, filekind, pathformat="MacOS"):
    return (
        "<FILE>"
        "<PATHTYPE>%s</PATHTYPE>"
        "<PATH>%s</PATH>"
        "<PATHFORMAT>%s</PATHFORMAT>"
        "<FILEKIND>%s</FILEKIND>"
        "<FILEFLAGS></FILEFLAGS>"
        "</FILE>"
    ) % (pathtype, escape(path), pathformat, filekind)


def fileref_entry(pathtype, path, pathformat="MacOS", targetname=None):
    """FILEREFs inside LINKORDER inherit the TARGET context; FILEREFs inside
    GROUPLIST live at PROJECT level and REQUIRE <TARGETNAME> (verified against
    the reference export — omitting it gives "A target needs to be specified
    in this context" at import)."""
    tn = "<TARGETNAME>%s</TARGETNAME>" % escape(targetname) if targetname else ""
    return (
        "<FILEREF>" + tn +
        "<PATHTYPE>%s</PATHTYPE>"
        "<PATH>%s</PATH>"
        "<PATHFORMAT>%s</PATHFORMAT>"
        "</FILEREF>"
    ) % (pathtype, escape(path), pathformat)


def convert_file_path(raw_path, prefix):
    """Return (pathtype, converted_path, is_library, repo_relative_path_or_None),
    or None if the file was deleted from the repo (drop from project)."""
    if raw_path in MANIFEST_RENAMES:
        target = MANIFEST_RENAMES[raw_path]
        if target is None:
            return None  # dropped
        # build Mac absolute path from the repo-relative target
        prefix_parts = prefix.split(":")
        tree_parts = MCP_TREE_LOCATION.split("/")
        source_root = prefix_parts[:-len(tree_parts)]
        mac_path = ":".join(source_root + target.split("/"))
        return ("Absolute", mac_path, False, target)
    base = posixpath.basename(raw_path)
    # Library entries (no directory, .Lib/CarbonLib) -> Name refs via SystemSearchPaths
    if base.endswith(".Lib") or base == "CarbonLib" or "/" not in raw_path and not base.endswith(".c"):
        return ("Name", base, True, None)
    if "MacOS Support/" in raw_path:  # CW-installation library given by sub-path
        return ("Name", base, True, None)
    mac_path, repo_rel = unix_rel_to_mac_abs(raw_path, prefix)
    return ("Absolute", mac_path, False, repo_rel)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--manifest", default=os.path.normpath(DEFAULT_MANIFEST))
    ap.add_argument("--reference", default=os.path.normpath(DEFAULT_REFERENCE))
    ap.add_argument("--out", default=None)
    ap.add_argument("--prefix", default=DEFAULT_PREFIX)
    ap.add_argument("--ideversion", default="5.0")
    ap.add_argument("--lf", action="store_true", help="LF endings (default CR)")
    args = ap.parse_args()

    out_path = args.out or os.path.join(os.path.dirname(args.manifest), "MacSurf-import.xml")

    # ---- prologue + DOCTYPE from the genuine reference ----------------------
    ref = open(args.reference, "r", encoding="utf-8", errors="replace").read()
    m = re.search(r"(<!DOCTYPE PROJECT \[.*?\]>)", ref, re.DOTALL)
    if not m:
        sys.exit("could not find DOCTYPE in reference export")
    doctype = m.group(1)

    # ---- parse the manifest --------------------------------------------------
    tree = ET.parse(args.manifest)
    root = tree.getroot()
    target = root.find(".//TARGET")
    target_name = target.findtext("NAME", "MacSurf")

    # scalar settings pass-through; access paths rebuilt
    user_paths, system_paths, scalars = [], [], []
    for s in target.find("SETTINGLIST").findall("SETTING"):
        name = s.findtext("NAME")
        value = s.findtext("VALUE")
        if name == "UserSearchPath":
            user_paths.append(convert_access_path(value, args.prefix))
        elif name == "SystemSearchPath":
            system_paths.append(convert_access_path(value, args.prefix))
        else:
            scalars.append((name, value))

    # Add access paths for every directory that contains a listed source file
    # (needed so #include "..." of in-tree headers next to sources also resolves,
    # and harmless otherwise). Rooted absolute, non-recursive.
    file_dirs = []

    # files
    files_xml, linkorder_xml = [], []
    n_lib = 0
    missing = []
    repo_root = os.path.normpath(os.path.join(os.path.dirname(args.manifest), "..", "..", "..", ".."))
    for f in target.find("FILELIST").findall("FILE"):
        raw = f.findtext("PATH")
        kind = f.findtext("FILEKIND", "Text")
        result = convert_file_path(raw, args.prefix)
        if result is None:
            continue  # file deleted from repo; drop from project
        ptype, conv, is_lib, repo_rel = result
        if is_lib:
            n_lib += 1
            files_xml.append(file_entry("Name", conv, "Library"))
        else:
            # validate the resolved path actually exists in the repo tree
            if repo_rel and not os.path.exists(os.path.join(repo_root, repo_rel)):
                missing.append((raw, repo_rel))
            files_xml.append(file_entry("Absolute", conv, kind))
            d = conv.rsplit(":", 1)[0] + ":"
            if d not in file_dirs:
                file_dirs.append(d)

    for fr in target.find("LINKORDER").findall("FILEREF"):
        raw = fr.findtext("PATH")
        result = convert_file_path(raw, args.prefix)
        if result is None:
            continue
        ptype, conv, is_lib, _ = result
        linkorder_xml.append(fileref_entry("Name" if is_lib else "Absolute", conv))

    # groups (mirror manifest groups; FILEREFs in proper form)
    groups_xml = []
    grouped = set()
    gl = root.find("GROUPLIST")
    if gl is not None:
        for g in gl.findall("GROUP"):
            gname = g.findtext("NAME", "Group")
            refs = []
            for fr in g.findall("FILEREF"):
                raw = fr.findtext("PATH")
                result = convert_file_path(raw, args.prefix)
                grouped.add(raw)
                if result is None:
                    continue
                ptype, conv, is_lib, _ = result
                refs.append(fileref_entry("Name" if is_lib else "Absolute", conv, targetname=target_name))
            groups_xml.append("<GROUP><NAME>%s</NAME>%s</GROUP>" % (escape(gname), "".join(refs)))
    # catch-all group for files not in any manifest group (CW shows the project tree from GROUPLIST)
    leftovers = []
    for f in target.find("FILELIST").findall("FILE"):
        raw = f.findtext("PATH")
        if raw in grouped:
            continue
        result = convert_file_path(raw, args.prefix)
        if result is None:
            continue
        ptype, conv, is_lib, _ = result
        leftovers.append(fileref_entry("Name" if is_lib else "Absolute", conv, targetname=target_name))
    if leftovers:
        groups_xml.append("<GROUP><NAME>Other Sources</NAME>%s</GROUP>" % "".join(leftovers))

    # ---- settings block ------------------------------------------------------
    settings = []
    for name, value in scalars:
        settings.append(settings_scalar(name, value))
    # genuine nested access-path structures
    up_blocks = [search_path_block(p, "MacOS", r) for (p, r) in user_paths]
    # also add the per-directory absolute paths (for in-tree header resolution)
    up_blocks += [search_path_block(d, "MacOS", "Absolute") for d in file_dirs]
    sp_blocks = [search_path_block(p, "MacOS", r) for (p, r) in system_paths]
    settings.append("<SETTING><NAME>UserSearchPaths</NAME>%s</SETTING>" % "".join(up_blocks))
    settings.append("<SETTING><NAME>SystemSearchPaths</NAME>%s</SETTING>" % "".join(sp_blocks))

    # ---- assemble -------------------------------------------------------------
    out = []
    out.append('<?xml version="1.0" encoding="UTF-8" standalone="yes" ?>')
    out.append('<?codewarrior exportversion="1.0.1" ideversion="%s" ?>' % args.ideversion)
    out.append(doctype)
    out.append("<PROJECT>")
    out.append("<TARGETLIST>")
    out.append("<TARGET>")
    out.append("<NAME>%s</NAME>" % escape(target_name))
    out.append("<SETTINGLIST>%s</SETTINGLIST>" % "".join(settings))
    out.append("<FILELIST>%s</FILELIST>" % "".join(files_xml))
    out.append("<LINKORDER>%s</LINKORDER>" % "".join(linkorder_xml))
    out.append("</TARGET>")
    out.append("</TARGETLIST>")
    out.append("<TARGETORDER><ORDEREDTARGET><NAME>%s</NAME></ORDEREDTARGET></TARGETORDER>" % escape(target_name))
    out.append("<GROUPLIST>%s</GROUPLIST>" % "".join(groups_xml))
    out.append("</PROJECT>")

    text = "\n".join(out)
    # validate well-formedness of what we produced (strip PI/DOCTYPE for ET)
    body = text.split("]>", 1)[1]
    ET.fromstring(body)

    eol = "\n" if args.lf else "\r"
    data = text.replace("\n", eol)
    with open(out_path, "w", encoding="utf-8", newline="") as fh:
        fh.write(data)

    n_files = len(files_xml)
    if missing:
        print("WARNING: %d manifest paths do not exist in the repo tree:" % len(missing))
        for raw, rel in missing[:15]:
            print("    %s  ->  %s" % (raw, rel))
        if len(missing) > 15:
            print("    ... and %d more" % (len(missing) - 15))
    print("wrote %s" % out_path)
    print("  target: %s | files: %d (libraries: %d) | linkorder: %d | groups: %d | "
          "user paths: %d (+%d per-dir) | system paths: %d"
          % (target_name, n_files, n_lib, len(linkorder_xml), len(groups_xml),
             len(user_paths), len(file_dirs), len(system_paths)))


if __name__ == "__main__":
    main()
