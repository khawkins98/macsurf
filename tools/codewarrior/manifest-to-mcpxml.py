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
    # --- stale-TWIN correction (NOT a missing file: BOTH copies exist in the repo) ---
    # CLAUDE.md "Known Gotchas" documents that the Mac compiles s_dispatch.c (renamed
    # in a2f5656d) and that dispatch.c is a stale snapshot re-added later by 02da50f5.
    # Building dispatch.c instead of s_dispatch.c reproduces the prop_dispatch table
    # desync crash (unmapped-memory PC, e.g. 0x68F168F0, in set_initial). The libdom
    # events/dispatch.c entry is a DIFFERENT file and is correct as-is.
    "../../../../libcss/src/select/dispatch.c": "browser/libcss/src/select/s_dispatch.c",
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


# The hand-maintained manifest uses some setting NAMEs that are not the genuine
# CodeWarrior panel keys, so CW silently ignores them on import. Map them to the
# real names (verified against the reference CW export). The prefix-file rename is
# load-bearing: without MWFrontEnd_C_prefixname, macsurf_prefix.h is never applied
# to any compile, and every "inline", bool/true/false, NSLOG, Carbon.h suppression
# guard, and __MACOS9__ define is missing -> a 100+ error cascade (the first being
# the bare "inline" keyword in the ported libparserutils sources).
SETTING_NAME_RENAMES = {
    "MWFrontEnd_C_prefix_file": "MWFrontEnd_C_prefixname",
}

# Settings the import MUST carry for a clean C89/Carbon build, emitted if the
# manifest didn't supply them under any (renamed) name. Value is the bare filename;
# CW resolves it through the user access paths (the macos9 folder is on them).
REQUIRED_SCALARS = {
    "MWFrontEnd_C_prefixname": "macsurf_prefix.h",
}

# Settings whose manifest VALUE we deliberately override on conversion (the manifest
# stays the maintainer's source of truth; we only flip these at import-build time).
# MWWarning_C_warn_implicitconv: the manifest has it 1 (on), which floods a clean
# build with ~27K benign "implicit arithmetic conversion" warnings from the ported
# libraries (libcss/libdom/duktape do tons of intentional int-width conversions),
# burying the handful of warnings that matter. 0 = off. The genuine CW reference
# export also ships this off.
SETTING_VALUE_OVERRIDES = {
    "MWWarning_C_warn_implicitconv": "0",
}

# Library basenames the manifest references under their old (CW Pro 7-era) names,
# which don't exist in CW Pro 8. CW8 renamed the MSL libraries (combined C+C++ into
# "All", underscore-separated). The linker resolves these by Name on the system
# search paths; the CW7 name "MSL C.Carbon.Lib" isn't shipped, so the link fails
# with "could not find or load". Map to the CW8 name that actually ships in
# {Compiler}:MacOS Support:Libraries:Runtime:Libs:. (Same stale-manifest class as
# the MANIFEST_RENAMES .c entries.)
LIBRARY_NAME_RENAMES = {
    "MSL C.Carbon.Lib": "MSL_All_Carbon.Lib",
}


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


def parse_access_paths_xml(path):
    """Read the genuine CW access-paths export ('Access Paths.xml' at repo root)
    and return (user_paths, system_paths) as lists of (mac_path, format, root,
    recursive). This is patrick's actual working set — complete and correct,
    unlike the stale manifest UserSearchPath/SystemSearchPath entries."""
    root = ET.parse(path).getroot()
    out = {"UserSearchPaths": [], "SystemSearchPaths": []}
    # Panel-level flags. AlwaysSearchUserPaths=true is REQUIRED or angle-bracket
    # includes (<stat.h>, <parserutils/errors.h>) never search the user paths —
    # which cascades into thousands of "file cannot be opened" errors.
    flags = {}
    for sl in root.iter("SETTINGLIST"):
        for s in sl.findall("SETTING"):
            nm = s.findtext("NAME")
            if nm in ("AlwaysSearchUserPaths", "InterpretDOSAndUnixPaths",
                      "RequireFrameworkStyleIncludes") and s.findtext("VALUE") is not None:
                flags[nm] = s.findtext("VALUE")
            if nm not in out:
                continue
            for wrap in s.findall("SETTING"):
                sp = None
                recursive = "false"
                for inner in wrap.findall("SETTING"):
                    if inner.findtext("NAME") == "SearchPath":
                        sp = inner
                    elif inner.findtext("NAME") == "Recursive":
                        recursive = inner.findtext("VALUE") or "false"
                if sp is None:
                    continue
                p = f = r = None
                for kv in sp.findall("SETTING"):
                    n, v = kv.findtext("NAME"), kv.findtext("VALUE")
                    if n == "Path": p = v
                    elif n == "PathFormat": f = v
                    elif n == "PathRoot": r = v
                if p is not None:
                    out[nm].append((p, f or "MacOS", r or "Absolute", recursive == "true"))
    return out["UserSearchPaths"], out["SystemSearchPaths"], flags


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
    base = LIBRARY_NAME_RENAMES.get(base, base)  # fix CW7->CW8 library names
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
    ap.add_argument("--access-paths", default=os.path.normpath(
        os.path.join(os.path.dirname(__file__), "..", "..", "Access Paths.xml")),
        help="genuine CW access-paths export to source search paths from")
    ap.add_argument("--ideversion", default="5.0")
    ap.add_argument("--lf", action="store_true", help="LF endings (default CR)")
    ap.add_argument("-v", "--verbose", action="store_true",
                    help="list byte-identical twins too (not just differing ones)")
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

    # scalar settings pass through; access paths come from the genuine export
    # (Access Paths.xml), NOT the stale manifest UserSearchPath/SystemSearchPath.
    scalars = []
    seen_names = set()
    for s in target.find("SETTINGLIST").findall("SETTING"):
        name = s.findtext("NAME")
        if name in ("UserSearchPath", "SystemSearchPath"):
            continue  # stale; replaced below
        if name == "OutputDirectory":
            continue  # manifest stores it flat (just a VALUE); CW needs the nested
                      # Path/PathFormat/PathRoot form -> emitted below, not passed through
        name = SETTING_NAME_RENAMES.get(name, name)  # fix wrong CW panel keys
        value = SETTING_VALUE_OVERRIDES.get(name, s.findtext("VALUE"))  # flip chosen values
        scalars.append((name, value))
        seen_names.add(name)
    # backfill settings the build can't omit (e.g. the prefix file) if absent
    for name, value in REQUIRED_SCALARS.items():
        if name not in seen_names:
            scalars.append((name, value))
            seen_names.add(name)
    user_paths, system_paths, ap_flags = parse_access_paths_xml(args.access_paths)

    # files
    files_xml, linkorder_xml = [], []
    n_lib = 0
    missing = []
    source_files = []  # (raw_manifest_path, repo_rel) for every non-library entry
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
            source_files.append((raw, repo_rel))
            files_xml.append(file_entry("Absolute", conv, kind))

    # ---- stale-twin detection -------------------------------------------------
    # A "twin" = the manifest references dir/base.c but dir/<prefix>_base.c ALSO
    # exists in the repo, where <prefix> is one of the CW8 flat-namespace rename
    # prefixes (sweeps a2f5656d, dc62be41, fixes57, fixes61...). The manifest may
    # be pointing at the stale copy -- which still compiles cleanly, so nothing
    # errors; it just builds old code. This is the dispatch.c/s_dispatch.c bug
    # class documented in CLAUDE.md "Known Gotchas". These need a MAINTAINER
    # decision (which copy is the real build file?) -- do NOT auto-fix; entries
    # already resolved by MANIFEST_RENAMES above are exempt (mapping documented).
    #
    # Twins whose content is byte-identical to the manifest's choice are reported
    # separately: building either produces the same code TODAY, but they will
    # drift the moment one is edited.
    RENAME_PREFIXES = ("p_", "s_", "ag_", "css_", "ns_", "hub_", "pu_", "dom_",
                       "libcss_", "libhubbub_", "html_", "macos9_")
    twins_differ, twins_identical = [], []
    for raw, repo_rel in source_files:
        if not repo_rel:
            continue
        if raw in MANIFEST_RENAMES:
            continue  # already mapped with documented evidence
        d, base = os.path.split(repo_rel)
        full_dir = os.path.join(repo_root, d)
        if not os.path.isdir(full_dir):
            continue
        for pfx in RENAME_PREFIXES:
            cand = pfx + base
            cand_path = os.path.join(full_dir, cand)
            if not os.path.exists(cand_path):
                continue
            ref_path = os.path.join(repo_root, repo_rel)
            try:
                same = (os.path.exists(ref_path) and
                        open(ref_path, "rb").read() == open(cand_path, "rb").read())
            except OSError:
                same = False
            entry = (repo_rel, posixpath.join(d, cand))
            (twins_identical if same else twins_differ).append(entry)

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
    # access-path panel flags (AlwaysSearchUserPaths=true is the critical one)
    for fname in ("AlwaysSearchUserPaths", "InterpretDOSAndUnixPaths", "RequireFrameworkStyleIncludes"):
        settings.append(settings_scalar(fname, ap_flags.get(
            fname, "true" if fname == "AlwaysSearchUserPaths" else "false")))
    # genuine nested access-path structures, straight from the real export
    up_blocks = [search_path_block(p, f, r, rec) for (p, f, r, rec) in user_paths]
    sp_blocks = [search_path_block(p, f, r, rec) for (p, f, r, rec) in system_paths]
    settings.append("<SETTING><NAME>UserSearchPaths</NAME>%s</SETTING>" % "".join(up_blocks))
    settings.append("<SETTING><NAME>SystemSearchPaths</NAME>%s</SETTING>" % "".join(sp_blocks))
    # OutputDirectory must be the nested Path/PathFormat/PathRoot form (the manifest
    # stores it as a flat VALUE, which CW reads as an invalid output dir and then
    # won't write the linked binary). Path ":" + PathRoot "Project" = the project
    # folder, matching the genuine CW reference export.
    settings.append(
        "<SETTING><NAME>OutputDirectory</NAME>"
        "<SETTING><NAME>Path</NAME><VALUE>:</VALUE></SETTING>"
        "<SETTING><NAME>PathFormat</NAME><VALUE>MacOS</VALUE></SETTING>"
        "<SETTING><NAME>PathRoot</NAME><VALUE>Project</VALUE></SETTING>"
        "</SETTING>")

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
    if twins_differ:
        print("WARNING: %d manifest entries have a renamed TWIN on disk WITH DIFFERENT" % len(twins_differ))
        print("  CONTENT -- the manifest may be building the stale copy (the")
        print("  dispatch.c/s_dispatch.c bug class, see CLAUDE.md Known Gotchas).")
        print("  Maintainer decision needed; project was generated with the manifest's")
        print("  choice as-is:")
        for ref, twin in twins_differ:
            print("    manifest builds: %s   DIFFERING twin: %s" % (ref, twin))
    if twins_identical:
        print("note: %d manifest entries have a byte-identical renamed twin on disk" % len(twins_identical))
        print("  (harmless today; will become the warning above the moment either copy")
        print("  is edited). Run with -v to list them." if not args.verbose else "  :")
        if args.verbose:
            for ref, twin in twins_identical:
                print("    manifest builds: %s   identical twin: %s" % (ref, twin))
    print("wrote %s" % out_path)
    print("  target: %s | files: %d (libraries: %d) | linkorder: %d | groups: %d | "
          "user paths: %d | system paths: %d (from %s)"
          % (target_name, n_files, n_lib, len(linkorder_xml), len(groups_xml),
             len(user_paths), len(system_paths), os.path.basename(args.access_paths)))


if __name__ == "__main__":
    main()
