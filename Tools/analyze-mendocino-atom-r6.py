#!/usr/bin/env python3
"""R6 offline audit: ACPI VFCT ATOM display tables and Linux IP Discovery.

Accepts the R6-Results-v3.zip collected on the actual machine. Does not
access /dev/mem, sysfs, PCI, firmware or registers, and does not write the VBIOS.
The output is descriptive evidence, not hardware initialization or X6000 support.
"""
import argparse
import json
import struct
import zipfile
from pathlib import Path


class ParseError(ValueError):
    pass


def u16(b, p):
    if p < 0 or p + 2 > len(b):
        raise ParseError(f"short read at offset 0x{p:X}")
    return struct.unpack_from('<H', b, p)[0]


def u32(b, p):
    if p < 0 or p + 4 > len(b):
        raise ParseError(f"short read at offset 0x{p:X}")
    return struct.unpack_from('<I', b, p)[0]


def bounded(b, off, size, desc):
    if off < 0 or size < 0 or off > len(b) or size > len(b) - off:
        raise ParseError(f"{desc} extends beyond image")


def extract_vfct(vfct):
    bounded(vfct, 0, 0x4C, 'VFCT header')
    if vfct[:4] != b'VFCT':
        raise ParseError('not ACPI VFCT')
    length = u32(vfct, 4)
    if length < 0x4C or length > len(vfct) or sum(vfct[:length]) % 256 != 0:
        raise ParseError('invalid VFCT size or ACPI checksum')
    off = u32(vfct, 0x34)
    bounded(vfct, off, 28, 'VFCT image header')
    vendor, device = u16(vfct, off+12), u16(vfct, off+14)
    if (vendor, device) != (0x1002, 0x1506):
        raise ParseError('VFCT first image is not Mendocino 1002:1506')
    rom_size = u32(vfct, off+24)
    bounded(vfct, off+28, rom_size, 'VFCT VBIOS')
    image = vfct[off+28:off+28+rom_size]
    if image[:2] != b'\x55\xaa':
        raise ParseError('VBIOS ROM signature absent')
    return image


def table(b, master, idx, name):
    off = u16(b, master+4+idx*2)
    if not off:
        return None
    bounded(b, off, 4, name+' header')
    n = u16(b, off)
    if n < 4:
        raise ParseError(name+' table too short')
    bounded(b, off, n, name)
    return off, n, b[off+2], b[off+3]


def parse_atom(image):
    bounded(image, 0, 0x4A, 'ATOM ROM')
    rom = u16(image, 0x48)
    bounded(image, rom, 0x24, 'ATOM ROM header')
    if image[rom+4:rom+8] not in (b'ATOM', b'MOTA'):
        raise ParseError('ATOM signature absent')
    master = u16(image, rom+0x20)
    bounded(image, master, 74, 'ATOM MasterDataTable v2.1')
    if (u16(image, master),image[master+2],image[master+3]) != (74,2,1):
        raise ParseError('unknown ATOM master data table layout')
    names = {4:'firmwareinfo',6:'lcd_info',22:'displayobjectinfo',27:'dce_info',30:'integratedsysteminfo'}
    ts = {name:table(image,master,i,name) for i,name in names.items()}
    result = {
        'rom_header_offset': f'0x{rom:04X}', 'master_table_offset': f'0x{master:04X}',
        'tables': {name: None if desc is None else {
            'offset':f'0x{desc[0]:04X}', 'size':desc[1], 'revision':f'{desc[2]}.{desc[3]}'
        } for name,desc in ts.items()},
    }
    disp = ts['displayobjectinfo']
    if disp is None or disp[2:] != (1,4):
        raise ParseError('expected DisplayObjectInfo v1.4')
    disp_off,disp_size,_,_ = disp
    if disp_size < 8:
        raise ParseError('display table too short')
    count=image[disp_off+6]
    if count>16 or count > (disp_size-8)//16:
        raise ParseError('display paths exceed table')
    paths=[]
    for i in range(count):
        at=disp_off+8+i*16
        connector, connrec, encoder, extencoder, encrec, extrec, tag,priority,reserved = struct.unpack_from('<7H2B',image,at)
        for record_offset in (connrec,encrec,extrec):
            if record_offset and record_offset+2>disp_size:
                raise ParseError('path record offset extends beyond DisplayObjectInfo')
        paths.append({
            'index':i,'connector_objid':f'0x{connector:04X}',
            'encoder_objid':f'0x{encoder:04X}',
            'device_tag':f'0x{tag:04X}',
            'lcd1':bool(tag & 0x2), 'dfp1':bool(tag & 0x8),
            'connector_record_offset':f'0x{connrec:04X}',
            'encoder_record_offset':f'0x{encrec:04X}',
        })
    result['display']={'supported_devices':f'0x{u16(image,disp_off+4):04X}', 'path_count':count,'paths':paths}
    dce=ts['dce_info']
    if dce and dce[2:] == (4,4):
        at,ln,_,_=dce
        if ln<48: raise ParseError('DCE v4.4 shorter than mandatory fields')
        result['dce44']={
            'display_caps':f'0x{u32(image,at+4):08X}',
            'bootup_dispclk_10khz':u32(image,at+8),
            'dce_refclk_10khz':u16(image,at+12),
            'dpphy_refclk_10khz':u16(image,at+36),
            'hw_chip_id':f'0x{u16(image,at+38):04X}',
            'dcn_min_version_field':image[at+40],
            'dcn_max_version_field':image[at+41],
            'max_display_pipes':image[at+42],
            'max_bootup_active_pipes':image[at+43],
            'max_display_phys':image[at+45],
            'max_aux_pairs':image[at+46],
        }
    return result


def parse_bar5(resource_text):
    rows=[s.split() for s in resource_text.splitlines() if s.strip()]
    if len(rows)<6:
        raise ParseError('missing PCI BAR resource lines')
    start,end = int(rows[5][0],16),int(rows[5][1],16)
    if end<start or start==0:
        raise ParseError('BAR5 absent or invalid')
    return {'start':f'0x{start:X}','end':f'0x{end:X}','size_bytes':end-start+1}


def parse_ip(report,bar5):
    entries=report['gpu'][0]['ip_discovery']
    uniq={}
    for e in entries:
        try:
            hwid=int(e['hw_id']);inst=int(e['instance'])
            major,minor,rev=int(e['major']),int(e['minor']),int(e['revision'])
            addrs=tuple(int(s,16) for s in e['base_addr'].splitlines() if s.strip())
        except (KeyError,ValueError,TypeError) as exc:
            raise ParseError(f'invalid IP discovery record: {exc}') from exc
        key=(hwid,inst,major,minor,rev,addrs)
        uniq[key]=e
    result=[]
    ids={11:'GC',271:'DMU',42:'SDMA0',1:'MP1',34:'MMHUB',150:'UMC'}
    for (hwid,inst,major,minor,rev,addrs),e in sorted(uniq.items()):
        if hwid not in ids: continue
        base=[]
        for addr in addrs:
            # Linux's SOC15 MMIO register offsets commonly use DWORD units.
            # This is only a mathematical aperture check; NOT proof of an
            # address being accessible through BAR5 or safe to read/write.
            byte_candidate=addr*4
            base.append({'discovery_base_dword':f'0x{addr:08X}',
                'byte_offset_if_dword':f'0x{byte_candidate:X}',
                'within_bar5_if_direct':byte_candidate<bar5['size_bytes']})
        result.append({'block':ids[hwid], 'hwid':hwid, 'instance':inst,
            'version':f'{major}.{minor}.{rev}', 'bases':base})
    return {'source_records':len(entries), 'unique_records':len(uniq),'selected_blocks':result,
            'note':'BAR5 aperture test is arithmetic only, not a validated register mapping'}


def analyse(data):
    atom=parse_atom(extract_vfct(data['acpi-vfct.bin']))
    bar5=parse_bar5(data['pci-resource-ranges.txt'].decode())
    ip=parse_ip(json.loads(data['mendocino-ip-discovery.json']),bar5)
    return {'scope':'offline read-only analysis (no register access)',
            'atom':atom,'pci_bar5':bar5,'ip_discovery':ip}


def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('archive',help='R6-Results-v3.zip, not the raw VBIOS')
    ap.add_argument('--output',help='Write JSON report here (never embeds VBIOS bytes)')
    args=ap.parse_args()
    with zipfile.ZipFile(args.archive) as z:
        payload={name:z.read(name) for name in ('acpi-vfct.bin','pci-resource-ranges.txt','mendocino-ip-discovery.json')}
    report=analyse(payload)
    out=json.dumps(report,indent=2,ensure_ascii=False)+'\n'
    if args.output:
        Path(args.output).write_text(out,encoding='utf-8')
    else:
        print(out,end='')


if __name__=='__main__':
    main()
