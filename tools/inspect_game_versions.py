"""Read-only local package structure audit; never exports license/key bytes."""
from pathlib import Path
import hashlib,json,struct,zipfile,argparse

def sfo(data):
    magic,version,keys,values,count=struct.unpack_from('<5I',data)
    if magic!=0x46535000 or count>(len(data)-20)//16:raise ValueError('Invalid SFO header')
    result={}
    for i in range(count):
        key,fmt,size,maximum,offset=struct.unpack_from('<HHIII',data,20+i*16)
        name=data[keys+key:data.index(0,keys+key)].decode()
        value=data[values+offset:values+offset+size]
        if len(value)!=size:raise ValueError('SFO data bounds')
        if name in ('TITLE_ID','APP_VER','CATEGORY','TITLE','CONTENT_ID','VERSION','PSP2_SYSTEM_VER'):
            result[name]=int.from_bytes(value,'little') if fmt==0x404 else value.rstrip(b'\0').decode(errors='replace')
    return result

def executable(data):
    return {'bytes':len(data),'sha256':hashlib.sha256(data).hexdigest(),
            'container':'SELF/SCE' if data[:4]==b'SCE\0' else 'ELF' if data[:4]==b'\x7fELF' else 'other',
            'limits':'Container recognition does not demonstrate successful SELF/PFS decryption or execution.'}

parser=argparse.ArgumentParser()
parser.add_argument('directory',type=Path)
parser.add_argument('--output',type=Path,required=True)
args=parser.parse_args()
report={'directory':str(args.directory),'folders':[],'archives':[],'limits':['No device or desktop game execution performed.','No trusted external checksums supplied: absence of corruption is not established.','License matching/decryption is unverified; no license/key bytes are exported.']}
for folder in sorted(args.directory.iterdir()):
    if not folder.is_dir() or not ('Mai233' in folder.name or 'NONPDRM' in folder.name):continue
    files=[p for p in folder.rglob('*') if p.is_file()]
    entry={'name':folder.name,'file_count':len(files),'total_bytes':sum(p.stat().st_size for p in files),'contents':[]}
    for p in sorted(folder.rglob('param.sfo')):
        base=p.parent.parent
        if p.parent.name!='sce_sys':continue
        item={'path':str(base.relative_to(folder)),'sfo':sfo(p.read_bytes())}
        eboot=base/'eboot.bin'
        item['eboot']=executable(eboot.read_bytes()) if eboot.exists() else {'missing':True}
        modules=base/'sce_module'
        item['sce_module']=[x.name for x in sorted(modules.glob('*')) if x.is_file()]
        license=base/'sce_sys/package/work.bin'
        item['license']={'work_bin_present':license.exists(),'bytes':license.stat().st_size if license.exists() else 0}
        item['gamedata_present']=(base/'gamedata.bin').exists()
        entry['contents'].append(item)
    report['folders'].append(entry)
for p in sorted(args.directory.glob('*PCSD00001*.VPK')):
    with zipfile.ZipFile(p) as z:
        names=z.namelist();bad=z.testzip()
        item={'name':p.name,'bytes':p.stat().st_size,'entries':len(names),'zip_crc_bad_entry':bad,'sfo':sfo(z.read('sce_sys/param.sfo')),'eboot':executable(z.read('eboot.bin')) if 'eboot.bin' in names else {'missing':True},'gamedata_present':'gamedata.bin' in names,'sce_module':[n for n in names if n.startswith('sce_module/') and not n.endswith('/')],'license':{'work_bin_present':'sce_sys/package/work.bin' in names}}
        item['standalone_base_candidate']=item['sfo'].get('CATEGORY')=='gd'
        report['archives'].append(item)
args.output.parent.mkdir(parents=True,exist_ok=True)
args.output.write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps(report,ensure_ascii=False,indent=2))
