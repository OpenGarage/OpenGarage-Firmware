#!/usr/bin/env python3
"""Prepare and verify immutable stock OpenGarage release bundles.

Crypto helpers adapted from OpenSprinkler/tools/firmware_release.py.
No compilation or remote writes: sign the exact independently tested artifact.
"""
import argparse, hashlib, json, os, re, shlex, shutil, subprocess, tempfile
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]
DEFAULT_PUBLIC_HEADER = ROOT / "OpenGarage/firmware_update_public_key.h"
TARGET = "og-esp8266-4m-v1"
PRODUCT = "opengarage-stock"
MAX_DESCRIPTOR_BYTES = 1024

def run(command, *, dry_run=False, capture_output=False):
    print("+", shlex.join(str(value) for value in command))
    if dry_run:
        return None
    return subprocess.run(
        [str(value) for value in command], check=True,
        capture_output=capture_output)


def public_point(private_key):
    result = subprocess.run(
        ["openssl", "ec", "-in", str(private_key), "-pubout",
         "-conv_form", "uncompressed", "-outform", "DER"],
        check=True, capture_output=True)
    point = result.stdout[-65:]
    if len(point) != 65 or point[0] != 0x04:
        raise RuntimeError("Unable to extract an uncompressed P-256 public key")
    return point


def public_point_from_header(path):
    path = Path(path).resolve()
    if not path.is_file():
        raise RuntimeError(f"Firmware public-key header not found: {path}")
    values = re.findall(r"0x([0-9a-fA-F]{2})", path.read_text(encoding="ascii"))
    point = bytes(int(value, 16) for value in values)
    if len(point) != 65 or point[0] != 0x04:
        raise RuntimeError(f"Expected one 65-byte P-256 public key in {path}")
    return point


def require_matching_key(private_key, public_header):
    private_key = Path(private_key).expanduser().resolve()
    if not private_key.is_file():
        raise RuntimeError(f"Private signing key not found: {private_key}")
    if public_point(private_key) != public_point_from_header(public_header):
        raise RuntimeError("Private signing key does not match the public key embedded in firmware")
    return private_key


def write_public_header(path, point):
    rows = []
    for offset in range(0, len(point), 8):
        rows.append("\t" + ", ".join(f"0x{value:02x}" for value in point[offset:offset + 8]))
    body = ",\n".join(rows)
    content = f"""#pragma once

#include <stdint.h>

// Public half of the offline OpenGarage firmware release key.
#define FW_UPDATE_PUBLIC_KEY_CONFIGURED 1

static const uint8_t FW_UPDATE_PUBLIC_KEY[65] = {{
{body}
}};
"""
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(content, encoding="ascii")


def keygen(args):
    private_key = Path(args.private_key).expanduser().resolve()
    if private_key.exists():
        raise RuntimeError(f"Refusing to overwrite private key: {private_key}")
    private_key.parent.mkdir(parents=True, exist_ok=True, mode=0o700)
    os.umask(0o077)
    run(["openssl", "ecparam", "-name", "prime256v1", "-genkey", "-noout",
         "-out", private_key])
    os.chmod(private_key, 0o600)
    write_public_header(args.public_header, public_point(private_key))
    print(f"Private key created at {private_key}")
    print("Back it up offline. Commit only the generated public-key header.")


def sha256(path):
    digest = hashlib.sha256()
    with Path(path).open("rb") as source:
        for block in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def public_key_der(point):
    # SubjectPublicKeyInfo for id-ecPublicKey + prime256v1 + uncompressed point.
    prefix = bytes.fromhex("3059301306072a8648ce3d020106082a8648ce3d030107034200")
    return prefix + point


def verify_signature(payload, signature_text, point):
    try:
        signature = bytes.fromhex(signature_text.decode("ascii").strip())
    except (UnicodeDecodeError, ValueError) as error:
        raise RuntimeError("Release signature is not valid hexadecimal DER") from error
    with tempfile.TemporaryDirectory() as directory:
        temporary = Path(directory)
        payload_path = temporary / "release.json"
        signature_path = temporary / "release.sig.der"
        public_der = temporary / "public.der"
        public_pem = temporary / "public.pem"
        payload_path.write_bytes(payload)
        signature_path.write_bytes(signature)
        public_der.write_bytes(public_key_der(point))
        run(["openssl", "pkey", "-pubin", "-inform", "DER", "-in", public_der,
             "-out", public_pem])
        run(["openssl", "dgst", "-sha256", "-verify", public_pem,
             "-signature", signature_path, payload_path])


def catalog_root(base_url):
    base = base_url.rstrip("/")
    return base if base.endswith("/v1") else base + "/v1"


def version_id(version):
    if type(version) is not int or not 126 <= version <= 255:
        raise ValueError('Stock updater releases require numeric version 126..255')
    return '.'.join(str(version))


def read_json(path):
    return json.loads(Path(path).read_text())


def image_valid(path):
    path = Path(path)
    size = path.stat().st_size
    header = path.read_bytes()[:16]
    if not 1024 <= size <= 1044464 or header[0] != 0xe9 or not 1 <= header[1] <= 16:
        raise ValueError('Invalid ESP8266 application image size/header')
    if not 0x40000000 <= int.from_bytes(header[4:8], 'little') < 0x40300000:
        raise ValueError('Image is not an ESP8266 application')
    # Header cannot prove the linker/FS layout. Release operator must use the
    # reviewed d1_mini build (eagle.flash.4m1m.ld); never infer layout from size.
    return path


def entry_for(descriptor):
    rid = descriptor['id']
    return {'id':rid, 'version':descriptor['version'], 'targets':list(descriptor['targets']),
            'descriptor':f'/v1/releases/{rid}/release.json',
            'signature':f'/v1/releases/{rid}/release.sig'}


def verify_tree(output, public_header):
    output = Path(output)
    catalog = read_json(output/'manifest.json')
    if catalog.get('schema') != 2 or not isinstance(catalog.get('releases'), list):
        raise ValueError('Invalid catalog')
    point = public_point_from_header(public_header)
    seen=set()
    for entry in catalog['releases']:
        rid=version_id(entry['version'])
        if rid != entry.get('id') or rid in seen:
            raise ValueError('Invalid/duplicate stable release ID')
        seen.add(rid)
        directory=output/'releases'/rid
        raw=(directory/'release.json').read_bytes()
        if len(raw)>MAX_DESCRIPTOR_BYTES:
            raise ValueError('Descriptor exceeds device limit')
        verify_signature(raw,(directory/'release.sig').read_bytes(),point)
        d=json.loads(raw)
        if d.get('schema')!=1 or d.get('product')!=PRODUCT or d.get('id')!=rid or d.get('version')!=entry['version']:
            raise ValueError('Descriptor identity mismatch')
        if set(d.get('targets',{})) != {TARGET} or entry != entry_for(d):
            raise ValueError('Unsupported target or catalog/descriptor mismatch')
        item=d['targets'][TARGET]
        filename=f'opengarage-{rid}-esp8266.bin'
        if item.get('file')!=f'/v1/releases/{rid}/{filename}' or item.get('min_flash')!=4194304:
            raise ValueError('Artifact path/flash constraint mismatch')
        image=image_valid(directory/filename)
        if item.get('size')!=image.stat().st_size or item.get('sha256')!=sha256(image):
            raise ValueError('Image size/checksum mismatch')
    return catalog


def prepare(args):
    key=require_matching_key(args.private_key,args.public_header)
    rid=version_id(args.version)
    source=image_valid(args.esp8266)
    output=Path(args.output_dir).resolve();output.mkdir(parents=True,exist_ok=True)
    catalog=verify_tree(output,args.public_header) if (output/'manifest.json').exists() else {'schema':2,'releases':[]}
    if any(e['id']==rid for e in catalog['releases']) or (output/'releases'/rid).exists():
        raise ValueError('Release ID already exists; refusing to replace immutable release')
    filename=f'opengarage-{rid}-esp8266.bin'
    d={'schema':1,'product':PRODUCT,'id':rid,'version':args.version,'targets':{
        TARGET:{'file':f'/v1/releases/{rid}/{filename}','size':source.stat().st_size,
                'sha256':sha256(source),'min_flash':4194304}}}
    raw=(json.dumps(d,separators=(',',':'))+'\n').encode('ascii')
    if len(raw)>MAX_DESCRIPTOR_BYTES:raise ValueError('Descriptor exceeds device limit')
    releases=output/'releases';releases.mkdir(exist_ok=True)
    with tempfile.TemporaryDirectory(dir=releases,prefix='.prepare-') as scratch:
        folder=Path(scratch)
        shutil.copyfile(source,folder/filename)
        if sha256(folder/filename)!=d['targets'][TARGET]['sha256']:
            raise ValueError('Source image changed while preparing release')
        (folder/'release.json').write_bytes(raw)
        run(['openssl','dgst','-sha256','-sign',key,'-out',folder/'signature.der',folder/'release.json'])
        signature=(folder/'signature.der').read_bytes().hex()+'\n'
        (folder/'signature.der').unlink()
        (folder/'release.sig').write_text(signature)
        verify_signature(raw,signature.encode(),public_point_from_header(args.public_header))
        # Refuse clobber; publication is intentionally a single-writer operation.
        destination=releases/rid;destination.mkdir()
        for f in folder.iterdir():shutil.move(str(f),destination/f.name)
    catalog['releases'].append(entry_for(d))
    catalog['releases'].sort(key=lambda e:e['version'],reverse=True)
    temp=output/'manifest.json.tmp';temp.write_text(json.dumps(catalog,separators=(',',':'))+'\n');temp.replace(output/'manifest.json')
    verify_tree(output,args.public_header)
    print(f'Prepared {rid}: {d["targets"][TARGET]["sha256"]}')


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    sub=parser.add_subparsers(dest='command',required=True)
    key=sub.add_parser('keygen');key.add_argument('--private-key',required=True)
    key.add_argument('--public-header',default=str(DEFAULT_PUBLIC_HEADER));key.set_defaults(func=keygen)
    prep=sub.add_parser('prepare');prep.add_argument('--private-key',required=True)
    prep.add_argument('--public-header',default=str(DEFAULT_PUBLIC_HEADER))
    prep.add_argument('--output-dir',required=True);prep.add_argument('--esp8266',required=True)
    prep.add_argument('--version',type=int,required=True);prep.set_defaults(func=prepare)
    check=sub.add_parser('verify');check.add_argument('--output-dir',required=True)
    check.add_argument('--public-header',default=str(DEFAULT_PUBLIC_HEADER))
    check.set_defaults(func=lambda a:print('Verified',len(verify_tree(a.output_dir,a.public_header)['releases']),'releases'))
    args=parser.parse_args()
    try:args.func(args)
    except (OSError,ValueError,RuntimeError,KeyError,TypeError,subprocess.CalledProcessError) as e:
        parser.exit(1,f'error: {e}\n')


if __name__=='__main__':main()
