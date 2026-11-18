#!/usr/bin/env python3
"""Offline WSS release-policy checks. This does not grant shipping approval.

The NSS audit is evidence about a pinned snapshot, not a revocation service.
Human review of vendor/browser advisories and endpoint chains is still required.
"""
import argparse
from datetime import date, datetime, timedelta, timezone
import hashlib
import importlib.util
import json
from pathlib import Path
import re

from cryptography import x509
from cryptography.hazmat.primitives import serialization

ROOT = Path(__file__).resolve().parents[1]


def load_generator():
    spec = importlib.util.spec_from_file_location('og_wss_generator', Path(__file__).with_name('generate_wss_store.py'))
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def metadata_hash(roots):
    return hashlib.sha256(json.dumps(roots, sort_keys=True, separators=(',', ':')).encode()).hexdigest()


def nss_audit(certdata, manifest):
    """Match DER certificates and SHA-1-linked trust objects, not display names."""
    certs, trusts = {}, {}

    def octets(obj, field):
        match = re.search(r'^' + field + r' MULTILINE_OCTAL\n(.*?)^END$', obj, re.M | re.S)
        if not match:
            raise ValueError('Missing NSS octet field: ' + field)
        return bytes(int(v, 8) for v in re.findall(r'\\([0-7]{3})', match.group(1)))

    for obj in re.split(r'(?=^CKA_CLASS CK_OBJECT_CLASS )', certdata, flags=re.M):
        if obj.startswith('CKA_CLASS CK_OBJECT_CLASS CKO_CERTIFICATE\n'):
            der = octets(obj, 'CKA_VALUE')
            fp = hashlib.sha256(der).hexdigest()
            constraint = re.search(r'^CKA_NSS_SERVER_DISTRUST_AFTER (.+)$', obj, re.M)
            certs[fp] = (hashlib.sha1(der).hexdigest(), constraint.group(1) if constraint else 'missing')
        elif obj.startswith('CKA_CLASS CK_OBJECT_CLASS CKO_NSS_TRUST\n'):
            key = octets(obj, 'CKA_CERT_SHA1_HASH').hex()
            value = re.search(r'^CKA_TRUST_SERVER_AUTH CK_TRUST (\S+)$', obj, re.M)
            if key in trusts:
                raise ValueError('Duplicate NSS trust object')
            trusts[key] = value.group(1) if value else 'missing'
    results = []
    for root in manifest['roots']:
        certificate = certs.get(root['sha256'])
        results.append({'name': root['name'], 'sha256': root['sha256'],
                        'present': certificate is not None,
                        'server_auth': trusts.get(certificate[0], 'missing') if certificate else 'missing',
                        'server_distrust_after': certificate[1] if certificate else 'missing'})
    return results


def check_catalog(root=ROOT, today=None, require_approval=True):
    root = Path(root)
    today = today or datetime.now(timezone.utc).date()
    folder = root / 'tools/wss-roots'
    manifest = json.loads((folder / 'manifest.json').read_text())
    policy = json.loads((folder / 'release-policy.json').read_text())
    audit = json.loads((folder / 'nss-audit.json').read_text())
    if policy.get('schema') != 1 or audit.get('schema') != 1:
        raise ValueError('Unsupported WSS policy/audit schema')
    version = manifest['catalog_version']
    if not re.fullmatch(r'[A-Za-z0-9._-]+', version) or 'synthetic' in version:
        raise ValueError('Invalid production catalog version')
    roots = manifest['roots']
    digest = metadata_hash(roots)
    if not roots or digest != manifest['catalog_metadata_sha256'] or digest != policy['catalog_metadata_sha256']:
        raise ValueError('WSS catalog metadata changed without policy review')
    if policy['catalog_version'] != version:
        raise ValueError('WSS policy version mismatch')
    reviewed, expires = date.fromisoformat(policy['reviewed_on']), date.fromisoformat(policy['review_expires'])
    # A policy cannot extend its own freshness indefinitely. Refresh the evidence.
    if not reviewed <= today <= expires or not reviewed <= expires <= reviewed + timedelta(days=90):
        raise ValueError('WSS trust-policy review is stale or future-dated')
    if audit['source'] != policy['nss_source'] or date.fromisoformat(audit['reviewed_on']) != reviewed:
        raise ValueError('NSS audit source/date mismatch')
    source = audit['source']
    if (not re.fullmatch(r'[0-9a-f]{40}', source['commit']) or
            source['url'] != 'https://raw.githubusercontent.com/nss-dev/nss/' + source['commit'] + '/lib/ckfw/builtins/certdata.txt' or
            not re.fullmatch(r'[0-9a-f]{64}', source['sha256'])):
        raise ValueError('NSS evidence must identify an immutable source')
    names = [r['name'] for r in roots]
    if len(set(names)) != len(names) or any(not re.fullmatch(r'[A-Za-z0-9_.-]+', name) for name in names):
        raise ValueError('Duplicate/unsafe root name')
    expected = {r['name']: r['sha256'] for r in roots}
    rows = audit['roots']
    if len(rows) != len(roots) or {r['name']: r['sha256'] for r in rows} != expected:
        raise ValueError('NSS audit does not cover exactly the selected roots')
    for row in rows:
        if row['present'] is not True or row['server_auth'] != 'CKT_NSS_TRUSTED_DELEGATOR' or row['server_distrust_after'] != 'CK_BBOOL CK_FALSE':
            raise ValueError('Root is not unconditionally TLS-trusted in the reviewed NSS snapshot: ' + row['name'])
    pems = []
    for item in roots:
        pem = (folder / (item['name'] + '.pem')).read_bytes()
        cert = x509.load_pem_x509_certificate(pem)
        cert.verify_directly_issued_by(cert)
        der = cert.public_bytes(serialization.Encoding.DER)
        if hashlib.sha256(der).hexdigest() != item['sha256']:
            raise ValueError('Pinned root changed: ' + item['name'])
        # The oldest supported host cryptography exposes naive UTC properties.
        before = cert.not_valid_before_utc if hasattr(cert, 'not_valid_before_utc') else cert.not_valid_before
        after = cert.not_valid_after_utc if hasattr(cert, 'not_valid_after_utc') else cert.not_valid_after
        start, end = before.date(), after.date()
        # Maintain at least a 90-day release-planning horizon; this is not a
        # promise that installed firmware automatically enforces root expiry.
        if today < start or today + timedelta(days=90) >= end:
            raise ValueError('Root validity needs maintenance: ' + item['name'])
        if after.replace(tzinfo=None).isoformat() != item['not_after']:
            raise ValueError('Root expiry metadata mismatch: ' + item['name'])
        retirement = policy['tls_retirements'].get(item['name'])
        if retirement and today + timedelta(days=90) >= date.fromisoformat(retirement['stop_shipping_on']):
            raise ValueError('Root TLS retirement needs maintenance: ' + item['name'])
        pems.append((item['name'], pem))
    if set(policy['tls_retirements']) - set(names):
        raise ValueError('Retirement policy refers to an absent root')
    generated = load_generator().generate(pems, version, expected)
    header = root / 'OpenGarage/lib/WebSockets/src/OGWssPublicAnchors.h'
    if generated != header.read_text():
        raise ValueError('Generated WSS header differs from the reviewed catalog')
    if require_approval and (manifest.get('shipping_approved') is not True or
            any(r.get('shipping_approved') is not True for r in roots)):
        raise ValueError('WSS root catalog is not approved for shipping')
    return len(roots)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', type=Path, default=ROOT)
    parser.add_argument('--check-candidate', action='store_true', help='Validate inputs without granting/requiring shipping approval')
    parser.add_argument('--nss', type=Path, help='Reproduce the audit against the pinned certdata.txt (offline)')
    args = parser.parse_args()
    if args.nss:
        folder = args.root / 'tools/wss-roots'
        audit = json.loads((folder / 'nss-audit.json').read_text())
        data = args.nss.read_bytes()
        if hashlib.sha256(data).hexdigest() != audit['source']['sha256']:
            raise ValueError('NSS snapshot hash mismatch')
        manifest = json.loads((folder / 'manifest.json').read_text())
        if nss_audit(data.decode(), manifest) != audit['roots']:
            raise ValueError('NSS audit does not reproduce')
    count = check_catalog(args.root, require_approval=not args.check_candidate)
    print(f'PASS: {count} pinned WSS roots; ' + ('candidate checks only, not shipping approval' if args.check_candidate else 'release policy checked'))


if __name__ == '__main__':
    main()
