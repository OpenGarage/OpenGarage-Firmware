// Suggest the public endpoint until the user edits either custom field.
function bindCloudEndpointFields(byId) {
  const host = byId('bdmn'), port = byId('bprt');
  let edited = false;
  function suggest() {
    if (edited || byId('otcm').value !== '1') return;
    const name = host.value.toLowerCase();
    const publicPair = (name === 'ws.cloud.openthings.io' && port.value === '80') ||
                       (name === 'ws.openthings.io' && port.value === '443');
    if (!publicPair) return;
    const plain = byId('nossl').checked;
    host.value = plain ? 'ws.cloud.openthings.io' : 'ws.openthings.io';
    port.value = plain ? '80' : '443';
  }
  for (const field of [host, port]) field.addEventListener('input', () => { edited = true; });
  for (const field of ['otcm', 'nossl']) byId(field).addEventListener('change', suggest);
}
