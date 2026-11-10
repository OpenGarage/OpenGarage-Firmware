/* Shared helpers for the station pages (inlined by compress_htmls.mjs). No framework. */
'use strict';
const $ = id => document.getElementById(id);
const $$ = (sel, root) => Array.from((root || document).querySelectorAll(sel));

/* GET a JSON reply. URLs stay relative so pages also work through the OTC proxy. */
function getJSON(url, ms) {
  const ctl = new AbortController(), t = setTimeout(() => ctl.abort(), ms || 10000);
  return fetch(url, {signal: ctl.signal, cache: 'no-store'})
    .then(r => { if (!r.ok) throw new Error('HTTP ' + r.status); return r.json(); })
    .finally(() => clearTimeout(t));
}

/* Message line: text, class (ok / err / info), optional auto-clear in ms. */
function showMsg(el, text, cls, ms) {
  el = typeof el === 'string' ? $(el) : el;
  el.classList.remove('ok', 'err', 'info');
  if (cls) el.classList.add(cls);
  el.textContent = text;
  clearTimeout(el._t);
  if (ms > 0) el._t = setTimeout(() => { el.textContent = ''; }, ms);
}

/* Inputs with a clear (x) button. */
function wireClearButtons() {
  $$('.inp.clear').forEach(box => {
    const input = box.querySelector('input'), x = box.querySelector('.x');
    const sync = () => { x.hidden = !input.value; };
    x.addEventListener('click', () => { input.value = ''; input.dispatchEvent(new Event('input')); input.focus(); });
    input.addEventListener('input', sync); sync();
  });
}

/* Device key remembered in localStorage ('og_dkey'), as before. */
function wireDeviceKey(input, onCleared) {
  try { const k = localStorage.getItem('og_dkey'); if (k) input.value = k; } catch (e) {}
  input.addEventListener('input', () => {
    if (input.value !== '') return;
    try { localStorage.removeItem('og_dkey'); } catch (e) {}
    if (onCleared) onCleared();
  });
}
function saveDeviceKey(input) { try { localStorage.setItem('og_dkey', input.value); } catch (e) {} return input.value; }

/* Firmware version 127 -> "1.2.7" */
function fwLabel(v) { return (v / 100 >> 0) + '.' + (v / 10 % 10 >> 0) + '.' + (v % 10 >> 0); }

/* Show or hide every element matching a selector. */
function toggleAll(sel, show) { $$(sel).forEach(e => { e.hidden = !show; }); }

document.addEventListener('DOMContentLoaded', wireClearButtons);
