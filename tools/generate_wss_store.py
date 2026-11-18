#!/usr/bin/env python3
"""Generate reviewed, predecoded BearSSL anchors; no network access at build time."""
import argparse,hashlib,json,re
from pathlib import Path
from cryptography import x509
from cryptography.hazmat.primitives import serialization
from cryptography.hazmat.primitives.asymmetric import ec,rsa

def tlv(b,pos):
 start=pos;tag=b[pos];pos+=1;n=b[pos];pos+=1
 if n&128:
  count=n&127
  if not count or count>4:raise ValueError('Invalid DER length')
  n=int.from_bytes(b[pos:pos+count],'big');pos+=count
 end=pos+n
 if end>len(b):raise ValueError('Truncated DER')
 return tag,start,pos,end

def subject_der(der):
 # Preserve the certificate's exact encoded Name rather than re-encode it.
 _,_,outer,_=tlv(der,0);_,_,pos,_=tlv(der,outer)
 if der[pos]==0xa0:pos=tlv(der,pos)[3]
 for _ in range(4):pos=tlv(der,pos)[3] # serial, signature, issuer, validity
 tag,start,_,end=tlv(der,pos)
 if tag!=0x30:raise ValueError('Subject is not a sequence')
 return der[start:end]

def generate(pems,version,expected=None):
 entries=[];seen={};fingerprints=[]
 for name,pem in pems:
  c=x509.load_pem_x509_certificate(pem);der=c.public_bytes(serialization.Encoding.DER)
  fp=hashlib.sha256(der).hexdigest()
  if expected is not None and expected[name]!=fp:raise ValueError('Fingerprint mismatch: '+name)
  if not c.extensions.get_extension_for_class(x509.BasicConstraints).value.ca:raise ValueError('Not CA')
  if c.subject!=c.issuer:raise ValueError('Not self-issued root')
  dn=hashlib.sha256(subject_der(der)).digest();k=c.public_key();second=0
  if isinstance(k,rsa.RSAPublicKey):
   if k.key_size>4096:raise ValueError('RSA key too large')
   nums=k.public_numbers();a=nums.n.to_bytes((nums.n.bit_length()+7)//8,'big');b=nums.e.to_bytes((nums.e.bit_length()+7)//8,'big');payload=a+b;kind=1;first=len(a);second=len(b)
  elif isinstance(k,ec.EllipticCurvePublicKey):
   first={'secp256r1':23,'secp384r1':24,'secp521r1':25}[k.curve.name];payload=k.public_bytes(serialization.Encoding.X962,serialization.PublicFormat.UncompressedPoint);kind=2
  else:raise ValueError('Unsupported key')
  if len(payload)>515:raise ValueError('Key workspace overflow')
  if dn in seen:
   if seen[dn]!=(kind,first,second,payload):raise ValueError('Conflicting key for DN')
   continue
  seen[dn]=(kind,first,second,payload);entries.append((name,dn,payload,kind,first,second));fingerprints.append(fp)
 digest=hashlib.sha256(('\n'.join(fingerprints)).encode()).hexdigest()
 lines=['// Generated; edit pinned inputs, not this file.','#pragma once',f'static const char trustVersion[] PROGMEM = "{version}";',f'static const char trustHash[] PROGMEM = "{digest}";']
 def arr(n,data):lines.append('static const uint8_t '+n+'[] PROGMEM = {'+','.join('0x%02x'%x for x in data)+'};')
 for i,(name,dn,payload,*_) in enumerate(entries):
  lines.append('// '+name);arr('trustDn'+str(i),dn);arr('trustKey'+str(i),payload)
 lines.append('static const og_wss_trust::Entry trustEntries[] PROGMEM = {')
 for i,(_,dn,payload,kind,first,second) in enumerate(entries):lines.append('{trustDn%d,trustKey%d,%d,%d,%d,%d},'%(i,i,len(payload),first,second,kind))
 lines.append('};');return '\n'.join(lines)+'\n'

if __name__=='__main__':
 a=argparse.ArgumentParser();a.add_argument('--roots',type=Path);a.add_argument('--manifest',type=Path);a.add_argument('--fixture',type=Path);a.add_argument('--output',required=True,type=Path);args=a.parse_args()
 if args.fixture:
  pems=[('fixture'+str(i),x) for i,x in enumerate(re.findall(b'-----BEGIN CERTIFICATE-----.*?-----END CERTIFICATE-----',args.fixture.read_bytes(),re.S))]
  result=generate(pems,'synthetic-bench-only')
 else:
  m=json.loads(args.manifest.read_text());expected={r['name']:r['sha256'] for r in m['roots']};pems=[(name,(args.roots/(name+'.pem')).read_bytes()) for name in expected]
  result=generate(pems,m['catalog_version'],expected)
 args.output.write_text(result)
