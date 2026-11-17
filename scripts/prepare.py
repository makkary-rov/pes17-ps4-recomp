#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Prepare a native probe image from a game's plaintext eboot.

Input: eboot.elf (plain ELF64) if present, else plaintext eboot.bin (SELF).
The title config (--title, scripts/title_config.py) supplies the expected MD5 of
eboot.elf and the public-safe title facts. libc.prx is optional.
No decryption, patches to the game, third-party modules or Python dependencies.
Format references are linked in README.md. All offsets are checked before use.
"""
import argparse
import base64
import collections
import hashlib
import json
import re
from pathlib import Path
import struct
import title_config


def span(data, offset, size):
    if offset < 0 or size < 0 or offset + size > len(data):
        raise ValueError(f"out of bounds: {offset:#x}+{size:#x} / {len(data):#x}")
    return data[offset:offset + size]


def unpack(fmt, data, offset):
    return struct.unpack(fmt, span(data, offset, struct.calcsize(fmt)))


def parse_self(data):
    if span(data, 0, 4) != b'O\x15=\x1d':
        raise ValueError("expected PS4 SELF")
    count, = unpack('<H', data, 24)
    base = 32 + count * 32
    header = unpack('<16sHHIQQQIHHHHHH', data, base)
    if header[0][:7] != b'\x7fELF\x02\x01\x01' or header[2] != 62:
        raise ValueError("expected little-endian x86-64 ELF")
    if header[9] != 56 or not 0 < header[10] < 256:
        raise ValueError("unsupported program headers")
    ph = [dict(zip(('type', 'flags', 'offset', 'vaddr', 'paddr', 'filesz', 'memsz', 'align'),
                   unpack('<IIQQQQQQ', data, base + header[5] + i * 56)))
          for i in range(header[10])]
    end = max(p['offset'] + p['filesz'] for p in ph)
    if end > 512 * 1024 * 1024:
        raise ValueError("probe image exceeds 512 MiB limit")
    elf = bytearray(end)
    header_end = header[5] + header[10] * 56
    elf[:header_end] = span(data, base, header_end)
    covered = [(0, header_end)]
    segments = []
    for i in range(count):
        flags, off, size, mem = unpack('<QQQQ', data, 32 + i * 32)
        segments.append(dict(flags=hex(flags), offset=off, size=size))
        if not flags & 0x800:
            continue
        if flags & 10:
            raise ValueError("encrypted/compressed SELF segment is unsupported")
        index = (flags >> 20) & 4095
        if index >= len(ph):
            raise ValueError("invalid segment index")
        p = ph[index]
        if size != p['filesz'] or size != mem:
            raise ValueError("unsupported blocked segment layout")
        elf[p['offset']:p['offset'] + size] = span(data, off, size)
        covered.append((p['offset'], p['offset'] + size))
    missing = []
    for i, p in enumerate(ph):
        if p['filesz'] and not any(a <= p['offset'] and p['offset'] + p['filesz'] <= z
                                  for a, z in covered):
            missing.append(i)
            if p['type'] in (1, 2, 0x61000000, 0x61000010):
                raise ValueError(f"required segment {i} is unavailable")
    return elf, header, ph, segments, missing


def parse_elf(data):
    """Plain little-endian ELF64 x86-64: file offsets are used directly."""
    if span(data, 0, 7) != b'\x7fELF\x02\x01\x01':
        raise ValueError("expected little-endian ELF64")
    header = unpack('<16sHHIQQQIHHHHHH', data, 0)
    if header[2] != 62:
        raise ValueError("expected x86-64 ELF")
    if header[9] != 56 or not 0 < header[10] < 256:
        raise ValueError("unsupported program headers")
    ph = [dict(zip(('type', 'flags', 'offset', 'vaddr', 'paddr', 'filesz', 'memsz', 'align'),
                   unpack('<IIQQQQQQ', data, header[5] + i * 56)))
          for i in range(header[10])]
    return data, header[4], ph


def encode_id(value):
    """Same encoding as link_libc.py (copied: link_libc imports this module)."""
    alphabet = 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+-'
    result = alphabet[value & 63]
    value >>= 6
    while value:
        result = alphabet[value & 63] + result
        value >>= 6
    return result


CANONICAL_RE = re.compile(r'^[^#\s]+#[^#\s]+#[^#\s]+$')


def build_import_aliases(names, identities, config):
    """Return the import-aliases.txt text: one '<import> <canonical>' line per aliased import,
    in import table order. A suffix-map row aliases each import with its suffix, but only after
    the import's own decoded library and module equal the row's. Explicit import_aliases rows
    name an import directly; it must exist and no suffix row may cover it as well."""
    rows = {row['suffix']: row for row in config['import_suffix_map']}
    chosen = {}
    for name in names:
        parts = name.split('#')
        row = rows.get('#' + '#'.join(parts[1:])) if len(parts) == 3 else None
        if row is None:
            continue
        _, library, module = identities.get(name, [None, None, None])
        if library is None or module is None:
            raise ValueError(f"import {name}: library/module not decoded, but suffix map row "
                             f"{row['suffix']} needs them")
        if (library, module) != (row['library'], row['module']):
            raise ValueError(f"import {name}: decoded {library}/{module}, suffix map row "
                             f"{row['suffix']} says {row['library']}/{row['module']}")
        if name in row.get('except', ()):
            continue  # resolves by its own name
        chosen[name] = parts[0] + row['canonical']
    present = set(names)
    explicit = {}
    for index, row in enumerate(config['import_aliases']):
        where = f"import_aliases[{index}]"
        if row['import'] not in present:
            raise ValueError(f"{where}: import {row['import']!r} is not in this eboot's import table")
        if row['import'] in chosen:
            raise ValueError(f"{where}: import {row['import']!r} is already covered by import_suffix_map")
        if not CANONICAL_RE.match(row['canonical']):
            raise ValueError(f"{where}.canonical: must be NID#library#module, got {row['canonical']!r}")
        explicit[row['import']] = row['canonical']
    lines = []
    for name in dict.fromkeys(names):
        canonical = chosen.get(name, explicit.get(name))
        if canonical is None:
            continue
        if re.search(r'\s', name):
            raise ValueError(f"import {name!r}: whitespace is not allowed in an aliased import name")
        lines.append(f"{name} {canonical}\n")
    return ''.join(lines)


def read_input(game):
    """Return (name, container, source bytes, elf bytes, entry, program headers, segments, missing)."""
    if (game / 'eboot.elf').is_file():
        source = (game / 'eboot.elf').read_bytes()
        elf, entry, ph = parse_elf(source)
        return 'eboot.elf', 'elf', source, elf, entry, ph, [], []
    source = (game / 'eboot.bin').read_bytes()
    elf, header, ph, segments, missing = parse_self(source)
    return 'eboot.bin', 'self', source, elf, header[4], ph, segments, missing


def sfo(data):
    magic, version, keys, values, count = unpack('<4sIIII', data, 0)
    if magic != b'\0PSF':
        raise ValueError('bad SFO signature')
    result = {}
    for i in range(count):
        key, fmt, size, capacity, off = unpack('<HHIII', data, 20 + i * 16)
        k = data[keys + key:data.index(0, keys + key)].decode()
        v = span(data, values + off, size)
        result[k] = struct.unpack('<I', v)[0] if fmt == 0x404 else v.rstrip(b'\0').decode('utf8', 'replace')
    return result


def nid(name):
    salt = bytes.fromhex('518d64a635ded8c1e6b039b1c3e55230')
    digest = hashlib.sha1(name.encode('ascii') + salt).digest()[:8][::-1]
    return base64.b64encode(digest).decode().rstrip('=').replace('/', '-')


def inspect_libc(path):
    """Prove that this dump's _init_env is exactly RET; never assume it."""
    source = path.read_bytes()
    elf, header, ph, _, missing = parse_self(source)
    dynamic = next(p for p in ph if p['type'] == 2)
    tags = dict(unpack('<QQ', elf, pos) for pos in
                range(dynamic['offset'], dynamic['offset'] + dynamic['filesz'], 16))
    lib = next(p for p in ph if p['type'] == 0x61000000)
    blob = span(elf, lib['offset'], lib['filesz'])
    strings = span(blob, tags[0x61000035], tags[0x61000037])
    syms = span(blob, tags[0x61000039], tags[0x6100003f])
    evidence = {'sha256': hashlib.sha256(source).hexdigest(), 'init_env_is_ret': False}
    for pos in range(0, len(syms), 24):
        name, info, other, section, value, size = unpack('<IBBHQQ', syms, pos)
        span(strings, name, 1)
        symbol = strings[name:strings.index(0, name)].decode('ascii')
        key = symbol.split('#')[0]
        if key not in (nid('_init_env'), nid('_ZNSt8ios_base4InitC1Ev')) or not section or info & 15 != 2:
            continue
        p = next(p for p in ph if p['type'] == 1 and p['flags'] & 1
                 and p['vaddr'] <= value < p['vaddr'] + p['filesz'])
        code = span(elf, p['offset'] + value - p['vaddr'], size)
        if key == nid('_init_env'):
            evidence.update(symbol=symbol, address=hex(value), size=size, bytes=code.hex(),
                            init_env_is_ret=code == b'\xc3')
        else:
            evidence['ios_base_init'] = dict(symbol=symbol, address=hex(value), size=size,
                                             code_sha256=hashlib.sha256(code).hexdigest(),
                                             name='_ZNSt8ios_base4InitC1Ev')
    return evidence


def prepare(game, out, title, resource_inventory=True, allow_hash_mismatch=False):
    config = title_config.load(title)
    input_name, container, source, elf, entry, ph, segments, missing = read_input(game)
    input_md5 = hashlib.md5(source).hexdigest()
    hash_check = dict(status='not_checked', expected_md5=config['eboot_elf_md5'], input_md5=input_md5,
                      allowed_mismatch=False)
    if input_name == 'eboot.elf' and input_md5 != config['eboot_elf_md5']:
        if not allow_hash_mismatch:
            raise ValueError(f"eboot.elf md5 {input_md5} does not match title config "
                             f"{config['eboot_elf_md5']}; pass --allow-hash-mismatch to continue")
        hash_check.update(status='mismatch', allowed_mismatch=True)
    elif input_name == 'eboot.elf':
        hash_check['status'] = 'match'
    loads = [p for p in ph if p['type'] in (1, 0x61000010)]
    def mapped(addr, size=8):
        return any(p['vaddr'] <= addr and addr + size <= p['vaddr'] + p['memsz'] for p in loads)
    size = max(p['vaddr'] + p['memsz'] for p in loads)
    if size > 512 * 1024 * 1024 or not mapped(entry, 1):
        raise ValueError('invalid memory image')
    image = bytearray(size)
    for p in loads:
        if p['filesz'] > p['memsz']:
            raise ValueError('segment file size exceeds memory size')
        image[p['vaddr']:p['vaddr'] + p['filesz']] = span(elf, p['offset'], p['filesz'])
    dp = next(p for p in ph if p['type'] == 2)
    dyn = []
    for pos in range(dp['offset'], dp['offset'] + dp['filesz'], 16):
        tag, value = unpack('<QQ', elf, pos)
        if tag == 0:
            break
        dyn.append((tag, value))
    tags = dict(dyn)
    lib = next(p for p in ph if p['type'] == 0x61000000)
    blob = span(elf, lib['offset'], lib['filesz'])
    strings = span(blob, tags[0x61000035], tags[0x61000037])
    def string(off):
        span(strings, off, 1)
        return strings[off:strings.index(0, off)].decode('ascii')
    syms = span(blob, tags[0x61000039], tags[0x6100003f])
    if len(syms) % 24:
        raise ValueError('bad symbol table size')
    symbols = []
    for pos in range(0, len(syms), 24):
        name, info, other, shndx, value, sz = unpack('<IBBHQQ', syms, pos)
        symbols.append(dict(name=string(name), type=info & 15, section=shndx, value=value, size=sz))
    relocs, counts, imports, unmapped = [], collections.Counter(), {}, 0
    for offset_tag, size_tag in ((0x61000029, 0x6100002d), (0x6100002f, 0x61000031)):
        table = span(blob, tags[offset_tag], tags[size_tag])
        if len(table) % 24:
            raise ValueError('bad relocation table size')
        for pos in range(0, len(table), 24):
            target, info, addend = unpack('<QQq', table, pos)
            kind, sym = info & 0xffffffff, info >> 32
            counts[kind] += 1
            if not mapped(target):
                unmapped += 1
                continue
            if kind == 8:
                relocs.append((target, 0, addend, 0))  # base-relative
            elif kind in (1, 6, 7):
                symbol = symbols[sym]
                if symbol['section']:
                    if symbol['section'] == 0xfff1:
                        struct.pack_into('<Q', image, target, symbol['value'] + addend)
                    else:
                        relocs.append((target, 0, symbol['value'] + addend, 0))
                else:
                    if sym not in imports:
                        imports[sym] = len(imports)
                    kind = 2 if symbol['type'] == 1 else 1
                    if (kind == 1 and addend) or not 0 <= addend < 4096:
                        raise ValueError('unsupported import addend')
                    relocs.append((target, kind, imports[sym], addend))
            else:
                raise ValueError(f'unsupported relocation {kind}')
    if unmapped:
        raise ValueError(f'{unmapped} relocation(s) outside mapped segments of '
                         f'{sum(counts.values())} relocation records')
    names = [symbols[i]['name'] for i in imports]
    # Same decoding as link_modules.py: dynamic tags give library/module IDs; the
    # import name suffix '#<library key>#<module key>' selects them.
    libraries, modules = {}, {}
    for tag, value in dyn:
        if tag in (0x61000013, 0x61000015, 0x6100000d, 0x6100000f):
            table = libraries if tag in (0x61000013, 0x61000015) else modules
            key = encode_id(value >> 48)
            identity = (string(value & 0xffffffff), (value >> 32) & 65535)
            if key in table and table[key] != identity:
                raise ValueError('conflicting module/library IDs')
            table[key] = identity
    import_identities = {}
    for name in names:
        parts = name.split('#')
        if len(parts) == 3:
            lib, mod = parts[1], parts[2]
            import_identities[name] = [parts[0], libraries[lib][0] if lib in libraries else None,
                                       modules[mod][0] if mod in modules else None]
    aliases = build_import_aliases(names, import_identities, config)
    candidates = ('_init_env', 'atexit', 'exit', '_exit', '__cxa_atexit', '__cxa_finalize',
                  '__cxa_guard_acquire', '__cxa_guard_release', '__cxa_guard_abort',
                  '__stack_chk_guard', '__stack_chk_fail', 'malloc', 'free',
                  'memcpy', 'memmove', 'memset', 'memcmp', 'strlen', 'printf',
                  '_ZNSt8ios_base4InitC1Ev',
                  'sceKernelLoadStartModule', 'sceVideoOutOpen', 'sceKernelCreateSema', 'sceKernelWaitSema', 'sceKernelPollSema',
                  'sceKernelSignalSema', 'sceKernelCancelSema', 'sceKernelDeleteSema',
                  'sceAppContentInitialize', 'sceAppContentAppParamGetInt', 'sceAppContentGetAddcontInfoList',
                  'pthread_cond_init', 'sceSysmoduleLoadModule', 'pthread_mutexattr_init', 'gettimeofday',
                  'scePthreadMutexattrInit', 'scePthreadMutexattrSettype', 'scePthreadMutexattrDestroy',
                  'scePthreadMutexInit', 'scePthreadMutexLock', 'scePthreadMutexTrylock',
                  'scePthreadMutexUnlock', 'scePthreadMutexDestroy', 'scePthreadRwlockInit',
                  'scePthreadRwlockDestroy', 'scePthreadRwlockRdlock', 'scePthreadRwlockWrlock',
                  'scePthreadRwlockTryrdlock', 'scePthreadRwlockTrywrlock', 'scePthreadRwlockUnlock',
                  'scePthreadRwlockTimedrdlock', 'scePthreadRwlockTimedwrlock',
                  'sceKernelGetDirectMemorySize', 'sceKernelAllocateDirectMemory',
                  'sceKernelMapDirectMemory', 'sceKernelReleaseDirectMemory', 'sceKernelMunmap')
    known = {nid(name): name for name in candidates}
    libc_path = game / 'sce_module' / 'libc.prx'
    if libc_path.is_file():
        libc_evidence = dict(status='present', **inspect_libc(libc_path))
    else:
        libc_evidence = dict(status='absent', init_env_is_ret=False)
    capability = int(libc_evidence['init_env_is_ret'])
    image_sha256 = hashlib.sha256(image).hexdigest()
    out.mkdir(parents=True, exist_ok=True)
    # A deliberately small format, consumed by main.c; no host struct packing.
    with (out / 'boot.bin').open('wb') as f:
        f.write(struct.pack('<8sQQQQQQ', b'PESIMG1\0', size, entry, len(loads), len(relocs), len(names),
                            capability))
        for p in loads:
            f.write(struct.pack('<QQQ', p['vaddr'], p['memsz'], p['flags']))
        for name in names:
            encoded = name.encode()
            if len(encoded) >= 128:
                raise ValueError('import name too long')
            f.write(encoded.ljust(128, b'\0'))
        for relocation in relocs:
            f.write(struct.pack('<QQqq', *relocation))
        f.write(image)
    (out / 'eboot.elf').write_bytes(elf)
    (out / 'entry.bin').write_bytes(image[:1024])
    (out / 'import-aliases.txt').write_bytes(aliases.encode('utf-8'))
    resources = collections.Counter()
    total_bytes = 0
    for path in ((game / 'dvdroot_ps4').rglob('*') if resource_inventory else ()):
        if path.is_file():
            resources[path.relative_to(game / 'dvdroot_ps4').parts[0]] += 1
            total_bytes += path.stat().st_size
    report = dict(input_file=input_name, input_container=container,
                  input_sha256=hashlib.sha256(source).hexdigest(), input_md5=input_md5, input_bytes=len(source),
                  hash_check=hash_check, title_config=dict(path=str(title),
                                                           sha256=hashlib.sha256(Path(title).read_bytes()).hexdigest()),
                  sfo=sfo((game / 'sce_sys/param.sfo').read_bytes()), entry=hex(entry),
                  image_bytes=size, image_sha256=image_sha256, program_headers=ph, self_segments=segments,
                  unavailable_metadata_headers=missing, needed=[string(v) for t, v in dyn if t == 1],
                  relocation_counts=dict(counts), relocation_records_total=sum(counts.values()),
                  relocations_written=len(relocs), import_count=len(names), imports=names,
                  import_identities=import_identities,
                  import_name_hints={name: known[name.split('#')[0]] for name in names if name.split('#')[0] in known},
                  libc_evidence=libc_evidence,
                  bundled_modules=sorted(p.name for p in (game / 'sce_module').iterdir()) if (game / 'sce_module').is_dir() else [],
                  resources=dict(resources), resource_bytes=total_bytes, resource_inventory=resource_inventory,
                  status='Prepared only; execution and Vulkan are tested separately.')
    (out / 'analysis.json').write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n')
    print(f"{report['sfo'].get('TITLE')} | entry={entry:#x} | image={size:,} bytes | input={input_name}")
    print(f"{len(names)} imported symbols; {sum(counts.values()):,} relocation records; "
          f"{len(relocs):,} written; {len(report['needed'])} required modules")
    print(f"Unavailable non-loadable metadata headers: {missing}; not a byte-exact ELF reconstruction")
    print(f"Output: {out.resolve()}")
    print(f"libc status: {libc_evidence['status']}; _init_env verified RET: {libc_evidence['init_env_is_ret']}")


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('game', type=Path)
    parser.add_argument('--title', type=Path, required=True, help='title config (pes-title-1 JSON)')
    parser.add_argument('--out', type=Path, default=Path(__file__).resolve().parent.parent / 'out')
    parser.add_argument('--no-resource-inventory', action='store_true')
    parser.add_argument('--allow-hash-mismatch', action='store_true',
                        help='continue when eboot.elf does not match the title config MD5 (recorded)')
    args = parser.parse_args()
    try:
        prepare(args.game, args.out, args.title, not args.no_resource_inventory, args.allow_hash_mismatch)
    except (ValueError, OSError, StopIteration, KeyError, IndexError) as error:
        parser.exit(1, f'prepare failed: {error}\n')
