from pathlib import Path
import hashlib,json,os,plistlib,struct,sys,uuid,zipfile
p=Path(sys.argv[1])
with zipfile.ZipFile(p) as z:
    assert z.testzip() is None
    plists=[n for n in z.namelist() if n.endswith('.app/Info.plist')]
    assert len(plists)==1
    base=plists[0].rsplit('/',1)[0]
    info=plistlib.loads(z.read(plists[0]))
    assert info['CFBundleIdentifier']=='com.stevenlee.unchartedpsv'
    assert info['CFBundleExecutable']=='Vita3K-Uncharted-iOS'
    assert info['MinimumOSVersion']=='26.0'
    assert info['UIFileSharingEnabled']
    executable=z.read(base+'/'+info['CFBundleExecutable'])
    assert len(executable)>20*1024*1024, 'Unexpectedly small full-core executable'
    magic,cpu,sub,filetype,commands,size,flags,reserved=struct.unpack_from('<8I',executable)
    assert magic==0xfeedfacf and cpu==0x100000c and filetype==2
    pos=32;platform=None;ident=None
    for _ in range(commands):
        command,length=struct.unpack_from('<2I',executable,pos)
        assert length>=8 and pos+length<=32+size
        if command==0x32:platform=struct.unpack_from('<I',executable,pos+8)[0]
        if command==0x1b:ident=str(uuid.UUID(bytes=executable[pos+8:pos+24]))
        pos+=length
    assert platform==2, 'Not an iPhoneOS device executable'
    assert not any('/_CodeSignature/' in n or n.endswith('embedded.mobileprovision') for n in z.namelist())
    provenance=z.read(base+'/Uncharted-Provenance.txt').decode()
    assert 'PLUS_CORE=a86b5f93db43e4c4640ad1527e2a4695aad0f35b' in provenance
    if os.environ.get('GITHUB_SHA'):
        assert 'PORT_COMMIT='+os.environ['GITHUB_SHA']+'\n' in provenance
    shaders=0
    for source in Path('vita3k/shaders-builtin').rglob('*.spv'):
        name=base+'/shaders-builtin/'+source.relative_to('vita3k/shaders-builtin').as_posix()
        assert z.read(name)==source.read_bytes(), name
        shaders+=1
    assert shaders>0
    report={'ipa':p.name,'ipa_bytes':p.stat().st_size,'ipa_sha256':hashlib.sha256(p.read_bytes()).hexdigest(),'executable_bytes':len(executable),'mach_uuid':ident,'bundle_id':info['CFBundleIdentifier'],'minimum_ios':info['MinimumOSVersion'],'verified_builtin_spirv':shaders,'provenance':provenance,'device_rendering':'unverified'}
    p.with_suffix('.verification.json').write_text(json.dumps(report,indent=2))
    print(json.dumps(report,indent=2))
