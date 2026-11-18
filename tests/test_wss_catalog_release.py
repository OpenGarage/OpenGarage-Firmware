"""Offline trust-policy gates; disposable trees/keys only, no production signing."""
from datetime import date
import importlib.util
import json
from pathlib import Path
import shutil
import tempfile
from types import SimpleNamespace
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]


def load(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


c = load('catalog', ROOT / 'tools/check_wss_catalog.py')
r = load('release', ROOT / 'tools/firmware_release.py')
TODAY = date(2026, 11, 14)


def rejected(call, text):
    try:
        call()
    except ValueError as error:
        assert text in str(error), str(error)
        return
    raise AssertionError('Expected refusal: ' + text)


with tempfile.TemporaryDirectory() as temp:
    tree = Path(temp)
    folder = tree / 'tools/wss-roots'
    shutil.copytree(ROOT / 'tools/wss-roots', folder)
    header = tree / 'OpenGarage/lib/WebSockets/src/OGWssPublicAnchors.h'
    header.parent.mkdir(parents=True)
    shutil.copyfile(ROOT / 'OpenGarage/lib/WebSockets/src/OGWssPublicAnchors.h', header)
    paths = {name: folder / (name + '.json') for name in ('manifest', 'release-policy', 'nss-audit')}
    # Exercise both approval states independently of the source catalog's state.
    fixture = json.loads(paths['manifest'].read_text())
    fixture['shipping_approved'] = False
    for item in fixture['roots']:
        item['shipping_approved'] = False
    fixture['catalog_metadata_sha256'] = c.metadata_hash(fixture['roots'])
    paths['manifest'].write_text(json.dumps(fixture))
    policy = json.loads(paths['release-policy'].read_text())
    policy['catalog_metadata_sha256'] = fixture['catalog_metadata_sha256']
    paths['release-policy'].write_text(json.dumps(policy))
    originals = {name: path.read_text() for name, path in paths.items()}

    def edit(name, change):
        obj = json.loads(paths[name].read_text())
        change(obj)
        paths[name].write_text(json.dumps(obj))

    def reset():
        for name, value in originals.items():
            paths[name].write_text(value)

    def check(approval=False, today=TODAY):
        return c.check_catalog(tree, today, require_approval=approval)

    assert check() == 11
    rejected(lambda: check(True), 'not approved')
    # Approval validates the fixture guard without changing the source catalog.
    edit('manifest', lambda m: (m.update(shipping_approved=True),
                              [row.update(shipping_approved=True) for row in m['roots']]))
    m = json.loads(paths['manifest'].read_text())
    m['catalog_metadata_sha256'] = c.metadata_hash(m['roots'])
    paths['manifest'].write_text(json.dumps(m))
    edit('release-policy', lambda p: p.update(catalog_metadata_sha256=m['catalog_metadata_sha256']))
    assert check(True) == 11
    # Even with global approval, each root must have literal boolean True.
    approved_manifest = paths['manifest'].read_text()
    approved_policy = paths['release-policy'].read_text()
    for root_index in range(11):
        for approval in (None, False, 0, 1, 'true'):
            m = json.loads(approved_manifest)
            if approval is None:
                m['roots'][root_index].pop('shipping_approved')
            else:
                m['roots'][root_index]['shipping_approved'] = approval
            m['catalog_metadata_sha256'] = c.metadata_hash(m['roots'])
            paths['manifest'].write_text(json.dumps(m))
            edit('release-policy', lambda p: p.update(catalog_metadata_sha256=m['catalog_metadata_sha256']))
            rejected(lambda: check(True), 'not approved')
            paths['release-policy'].write_text(approved_policy)
    reset()

    for name, mutation, message in [
        ('manifest', lambda m: m['roots'][0].update(sha256='0' * 64), 'metadata changed'),
        ('release-policy', lambda p: p.update(catalog_version='other'), 'version mismatch'),
        ('release-policy', lambda p: p.update(review_expires='2035-01-01'), 'stale'),
        ('nss-audit', lambda a: a['source'].update(sha256='0' * 64), 'source/date mismatch'),
        ('nss-audit', lambda a: a['roots'].pop(), 'exactly'),
        ('nss-audit', lambda a: a['roots'][0].update(server_auth='CKT_NSS_MUST_VERIFY_TRUST'), 'not unconditionally'),
        ('nss-audit', lambda a: a['roots'][0].update(server_distrust_after='MULTILINE_OCTAL'), 'not unconditionally'),
        ('release-policy', lambda p: p['tls_retirements']['SSL.com_Root_Certification_Authority_RSA'].update(stop_shipping_on='2027-01-01'), 'retirement'),
    ]:
        edit(name, mutation)
        rejected(check, message)
        reset()
    rejected(lambda: check(today=date(2026, 11, 13)), 'future-dated')
    rejected(lambda: check(today=date(2027, 2, 13)), 'stale')
    original_header = header.read_text()
    header.write_text(original_header.replace('candidate-', 'synthetic-'))
    rejected(check, 'header differs')
    header.write_text(original_header)
    # A different valid root still fails the pin (its self-signature is valid).
    pem = folder / 'GTS_Root_R1.pem'
    original_pem = pem.read_bytes()
    pem.write_bytes((folder / 'ISRG_Root_X1.pem').read_bytes())
    rejected(check, 'Pinned root changed')
    pem.write_bytes(original_pem)
    # Advance a freshly reviewed policy to the retirement horizon.
    edit('release-policy', lambda p: p.update(reviewed_on='2028-10-03', review_expires='2028-12-01'))
    edit('nss-audit', lambda a: a.update(reviewed_on='2028-10-03'))
    rejected(lambda: check(today=date(2028, 10, 3)), 'retirement')
    reset()
    # prepare must refuse before dependency lookup, private-key access or output.
    args = SimpleNamespace(version=127, root=tree)
    with patch.object(r, 'require_pinned_dependencies', side_effect=AssertionError('dependency guard reached')), \
            patch.object(r, 'require_matching_key', side_effect=AssertionError('signing key reached')):
        # An unapproved or later-stale candidate must never reach either step.
        try:
            r.prepare(args)
        except ValueError:
            pass
        else:
            raise AssertionError('Unapproved catalog reached signing')

# Exercise the NSS parser with misleading labels and a DER-linked trust object.
der = b'root bytes'
octal = lambda b: ''.join('\\%03o' % n for n in b)
import hashlib
fixture = ('CKA_CLASS CK_OBJECT_CLASS CKO_CERTIFICATE\nCKA_LABEL UTF8 "wrong label"\n'
           'CKA_VALUE MULTILINE_OCTAL\n' + octal(der) + '\nEND\n'
           'CKA_NSS_SERVER_DISTRUST_AFTER CK_BBOOL CK_FALSE\n'
           'CKA_CLASS CK_OBJECT_CLASS CKO_NSS_TRUST\nCKA_CERT_SHA1_HASH MULTILINE_OCTAL\n' +
           octal(hashlib.sha1(der).digest()) + '\nEND\n'
           'CKA_TRUST_SERVER_AUTH CK_TRUST CKT_NSS_TRUSTED_DELEGATOR\n')
result = c.nss_audit(fixture, {'roots': [{'name': 'actual', 'sha256': hashlib.sha256(der).hexdigest()}]})
assert result[0]['present'] and result[0]['server_auth'] == 'CKT_NSS_TRUSTED_DELEGATOR'
print('PASS: WSS catalog pins, generated bytes, trust evidence, freshness, retirement, approval and pre-signing refusal')
