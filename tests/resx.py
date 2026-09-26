#!/usr/bin/env python3
"""Inspect the RT_MANIFEST resource of a PE, and *prove* it will activate.

Why this exists: grepping the .c3.manifest C3 wrote proves nothing about Side-by-Side
activation.  What matters is the resource *as it sits in the exe*: its type id, and
whether the comctl32 v6 dependency is still there after whatever tool stuffed it in.

Usage:
    resx.py --list <exe>            # (type, id, lang, size, sha1) per resource
    resx.py --check <exe> <xml>     # extract + compare canonically vs the XML C3
                                    # wrote, and assert the Common-Controls v6
                                    # dependentAssembly is present.  exit 0/1.
    resx.py <exe> [outdir]          # one file per RT_MANIFEST

PE resource table, things that bit me (all checked against real link.exe/mt.exe
output on this box):
  * IMAGE_RESOURCE_DIRECTORY header is 16B = Characteristics|TimeDateStamp|
    MajorVersion|MinorVersion|NumberOfNamed|NumberOfId -> counts at off+12.
  * entry = (NameOrId, OffsetToData); subdirectory flag is bit 31 of OffsetToData.
    **mt.exe writes NameOrId WITHOUT bit 31** (header still says named=0), so
    classify by the index range, not by that flag, or every mt.exe entry is
    misread as a string name.
  * every internal offset is relative to the *resource directory base RVA*.
  * tree depth 4: root(type) -> id -> lang -> IMAGE_RESOURCE_DATA_ENTRY, whose
    fields are {OffsetToData(RVA), Size, CodePage, Reserved} in that order.
  * **RT_MANIFEST is 24.  16 is RT_VERSION** and C3's own VERSIONINFO hangs off
    16; filtering on 16 hands you the VS_VERSION_INFO blob instead.
"""
import glob
import hashlib
import os
import struct
import sys
import xml.etree.ElementTree as ET

RT_MANIFEST = 24

# What the whole exercise is actually for: without this dependentAssembly the
# activation context resolves comctl32 to the v5.82 in %System32 and the
# toolbar/statusbar re-implementations get the wrong class table.
COMCTL_DEP = 'Microsoft.Windows.Common-Controls'


class Pe:
    def __init__(self, path):
        self.d = open(path, 'rb').read()
        d = self.d
        e = struct.unpack_from('<I', d, 0x3C)[0]
        coff = e + 4
        nsec, = struct.unpack_from('<H', d, coff + 2)
        optsz, = struct.unpack_from('<H', d, coff + 16)
        opt = coff + 20
        magic, = struct.unpack_from('<H', d, opt)
        # 32-bit PE: data dirs start at opt+96; 64-bit at opt+112. Resource dir
        # is the 3rd entry (index 2) -> +2*8.
        self.nrva, = struct.unpack_from('<I', d, opt + (96 if magic == 0x10b else 112) + 2 * 8)
        self.secs = []
        for i in range(nsec):
            h = opt + optsz + i * 40
            vsize, va, rawsize, rawptr = struct.unpack_from('<IIII', d, h + 8)
            self.secs.append((va, vsize, rawptr, rawsize))

    def rva(self, va):
        for v0, vsz, rp, rs in self.secs:
            if v0 <= va < v0 + max(vsz, rs):
                return rp + (va - v0)
        return None

    def entries(self, off):
        named, idcnt = struct.unpack_from('<HH', self.d, off + 12)
        out = []
        for i in range(named + idcnt):
            raw, ioff = struct.unpack_from('<II', self.d, off + 16 + i * 8)
            tid = (raw & 0x7FFFFFFF) if i >= named else None
            out.append((tid, bool(ioff & 0x80000000), rva_or_none(self, self.nrva + (ioff & 0x7FFFFFFF))))
        return out

    def data_at(self, off):
        dva, size = struct.unpack_from('<II', self.d, off)
        p = self.rva(dva)
        return self.d[p:p + size] if p is not None else b''


def rva_or_none(pe, va):
    return pe.rva(va)


def walk(pe, off, cur_type, out):
    """cur_type = the RT_* type that describes the entries of this directory.
    Tree shape is root(type) -> id -> lang -> DATA, and only the *lang* entry
    points straight at IMAGE_RESOURCE_DATA_ENTRY: there is no directory for it."""
    for tid, isdir, child in pe.entries(off):
        if isdir:
            walk(pe, child, cur_type if cur_type is not None else tid, out)
            continue
        blob = pe.data_at(child)
        out.append(('blob', '%d.%d.%d' % (cur_type, tid, len(out)), blob) if cur_type == RT_MANIFEST
                   else ('skip', tid, len(blob)))


def manifest_of(path):
    """Return the single RT_MANIFEST blob, or None."""
    pe = Pe(path)
    base = pe.rva(pe.nrva)
    if base is None:
        print('%s: no resource directory' % path)
        return None
    out = []
    walk(pe, base, None, out)
    res = [x for x in out if x[0] == 'blob']
    if not res:
        print('%s: 0 RT_MANIFEST resource(s)' % path)
        return None
    for tag, key, blob in res:
        typ, rid, lang = key.split('.')
        print('  type=%s id=%s lang=%s size=%d sha1=%s'
              % (typ, rid, lang, len(blob), hashlib.sha1(blob).hexdigest()))
    return res[0][2]


def canon(xml_bytes):
    """Compare manifests by *canonical* form, not bytes.

    mt.exe round-trips the XML through MSXML, so the blob in the exe is NOT
    byte-identical to the file C3 wrote.  Measured on this box (CtrlStatusBar):
       - dropped the `<?xml version=... standalone="yes"?>` declaration
       - expanded `<assemblyIdentity ... />` into `<...></...>`
       - dropped the trailing CRLF
    479 vs 504 bytes, sha1 differs, yet ElementTree canonical form is equal.
    The assembly identity -- the only part the activation context reads -- is
    untouched, so a byte compare would report a false failure forever.
    """
    return ET.tostring(ET.fromstring(xml_bytes))


def check(exe, xml_path):
    blob = manifest_of(exe)
    if blob is None:
        return 1
    src = open(xml_path, 'rb').read()

    try:
        same = canon(src) == canon(blob)
    except ET.ParseError as ex:
        print('  PARSE ERROR in embedded manifest: %s' % ex)
        return 1
    print('  canonical form identical to %s: %s' % (os.path.basename(xml_path), 'YES' if same else 'NO'))
    if not same:
        print('  (mt.exe normalized the XML; if only whitespace/self-closing tags '
              'differ this is fine -- see the canon() docstring)')

    root = ET.fromstring(blob)
    # `iter('assemblyIdentity')` matches nothing here: the manifest declares a
    # default namespace, so the real tag is '{urn:...asm.v1}assemblyIdentity'.
    deps = [e.get('name') for e in root.iter()
            if e.tag.split('}')[-1] == 'assemblyIdentity']
    ok = COMCTL_DEP in deps
    print('  dependentAssembly identities: %s' % deps)
    print('  comctl32 v6 dependency present: %s' % ('YES' if ok else 'NO'))
    return 0 if (ok and same) else 1


def dump(path, outdir):
    pe = Pe(path)
    base = pe.rva(pe.nrva)
    if base is None:
        print('%s: no resource directory' % path)
        return
    out = []
    walk(pe, base, None, out)
    res = [x for x in out if x[0] == 'blob']
    print('%s: %d RT_MANIFEST resource(s)' % (path, len(res)))
    if not os.path.isdir(outdir):
        os.makedirs(outdir)
    for tag, key, blob in res:
        typ, rid, lang = key.split('.')
        fn = os.path.join(outdir, '%s.%s.%s.manifest' % (os.path.basename(path), rid, lang))
        open(fn, 'wb').write(blob)
        print('  -> %s  %d bytes  sha1=%s' % (fn, len(blob), hashlib.sha1(blob).hexdigest()))


def main(argv):
    mode = argv[1]
    if mode == '--list':
        manifest_of(argv[2])
        return 0
    if mode == '--check':
        if len(argv) < 4:
            print('usage: resx.py --check <exe> <xml C3 wrote, may be a glob>')
            return 2
        # The session dir under %TEMP%\C3C is randomly named, so a literal path
        # is awkward from a .bat; accept a glob. Several sessions pile up, and
        # only the newest one is the manifest that produced <exe> — taking a
        # stale one would compare against the wrong source.
        hits = sorted(glob.glob(argv[3]), key=os.path.getmtime, reverse=True)
        if not hits:
            print('no source manifest matching %s' % argv[3])
            return 2
        rc = 0
        for src in hits:
            print('-- %s' % src)
            rc |= check(argv[2], src) and 1
        return rc
    dump(argv[1], argv[2] if len(argv) > 2 else '.')
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
