"""Release-tool checks use throwaway keys and an artificial image, never production signing."""
import importlib.util,json,tempfile,subprocess
from pathlib import Path
from types import SimpleNamespace
spec=importlib.util.spec_from_file_location('release',Path('tools/firmware_release.py'))
r=importlib.util.module_from_spec(spec);spec.loader.exec_module(r)
def fails(f):
    try:f()
    except (ValueError,RuntimeError,subprocess.CalledProcessError):return
    raise AssertionError('Expected rejection')
def fake_root(p, dep_line=None, revision=None, dirty=False):
    """A minimal tree with a resolved OTF git checkout, for the dependency guard."""
    lib=p/'OpenGarage/.pio/libdeps/d1_mini'/r.OTF_LIB;lib.mkdir(parents=True)
    git=lambda *a:subprocess.run(['git','-C',str(lib),*a],check=True,capture_output=True,text=True).stdout.strip()
    git('init','-q');(lib/'a.cpp').write_text('x')
    git('add','.');git('-c','user.name=t','-c','user.email=t@t','commit','-qm','x')
    pin=git('rev-parse','HEAD')
    if dirty:(lib/'a.cpp').write_text('changed')
    dep=dep_line or f'https://github.com/OpenThingsIO/{r.OTF_LIB}.git#{pin}'
    (p/'OpenGarage/platformio.ini').write_text(f'[env:d1_mini]\nlib_deps =\n\tbblanchon/ArduinoJson@7.1.0\n\t; note\n\t{dep}\nmonitor_speed = 1\n')
    (p/'OpenGarage/main.cpp').write_text('json += F(",\\"otf_revision\\":\\"%s\\"");\n'%(revision or pin))
    return pin
with tempfile.TemporaryDirectory() as t:
    p=Path(t)
    pin=fake_root(p/'ok');assert r.require_pinned_dependencies(p/'ok')==pin
    fake_root(p/'local',dep_line='symlink://../.local/otf-127');fails(lambda:r.require_pinned_dependencies(p/'local'))
    fake_root(p/'file',dep_line='file:///tmp/otf');fails(lambda:r.require_pinned_dependencies(p/'file'))
    fake_root(p/'branch',dep_line=f'https://github.com/OpenThingsIO/{r.OTF_LIB}.git#master');fails(lambda:r.require_pinned_dependencies(p/'branch'))
    fake_root(p/'rev',revision='0'*40);fails(lambda:r.require_pinned_dependencies(p/'rev'))
    fake_root(p/'dirty',dirty=True);fails(lambda:r.require_pinned_dependencies(p/'dirty'))
    fake_root(p/'stale',dep_line=f'https://github.com/OpenThingsIO/{r.OTF_LIB}.git#{"1"*40}',revision='1'*40)
    fails(lambda:r.require_pinned_dependencies(p/'stale'))
with tempfile.TemporaryDirectory() as t:
    p=Path(t);key=p/'key.pem';header=p/'public.h';fake_root(p/'root')
    r.keygen(SimpleNamespace(private_key=key,public_header=header))
    fails(lambda:r.keygen(SimpleNamespace(private_key=key,public_header=header)))
    assert key.stat().st_mode & 0o077 == 0
    b=bytearray(2048);b[0]=0xe9;b[1]=2;b[4:8]=(0x40100000).to_bytes(4,'little')
    image=p/'image.bin';image.write_bytes(b)
    args=SimpleNamespace(private_key=key,public_header=header,output_dir=p/'v1',esp8266=image,version=126,root=p/'root')
    r.prepare(args);c=r.verify_tree(p/'v1',header)
    assert c['releases'][0]['id']=='1.2.6' and 'build' not in c['releases'][0]
    assert c['releases'][0]['targets']==[r.TARGET]
    fails(lambda:r.prepare(args));fails(lambda:r.version_id(125))
    directory=p/'v1/releases/1.2.6';artifact=directory/'opengarage-1.2.6-esp8266.bin'
    artifact.write_bytes(bytes(b)+b'bad');fails(lambda:r.verify_tree(p/'v1',header));artifact.write_bytes(b)
    descriptor=directory/'release.json';raw=descriptor.read_bytes()
    descriptor.write_bytes(raw.replace(b'opengarage-stock',b'opensprinkler'))
    fails(lambda:r.verify_tree(p/'v1',header));descriptor.write_bytes(raw)
    # Even correctly signed metadata must enforce product/target identity.
    wrong=json.loads(raw);wrong['product']='other';descriptor.write_text(json.dumps(wrong))
    subprocess.run(['openssl','dgst','-sha256','-sign',str(key),'-out',str(p/'sig'),str(descriptor)],check=True)
    (directory/'release.sig').write_text((p/'sig').read_bytes().hex())
    fails(lambda:r.verify_tree(p/'v1',header))
    key2=p/'key2.pem';header2=p/'public2.h';r.keygen(SimpleNamespace(private_key=key2,public_header=header2))
    fails(lambda:r.require_matching_key(key2,header))
    b[4:8]=(0x40380000).to_bytes(4,'little');image.write_bytes(b)
    fails(lambda:r.image_valid(image))
print('Release tooling: pinned dependencies, single target, no build number, signatures, tampering, identity and immutability PASS')
