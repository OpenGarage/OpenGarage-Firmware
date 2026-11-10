// tools/compress_htmls.mjs
import { promises as fs } from "fs";
import path from "path";
import zlib from "zlib";
import crypto from "crypto";
import { minify } from "html-minifier-terser";

const WEB_DIR = path.resolve("html");        // <-- adjust if needed
const OUT_H   = path.resolve("./htmls.h");// output header

const MINIFY_OPTIONS = {
  collapseWhitespace: true,
  removeComments: true,
  removeRedundantAttributes: true,
  removeEmptyAttributes: true,
  removeScriptTypeAttributes: true,
  removeStyleLinkTypeAttributes: true,
  minifyCSS: true,
  minifyJS: true,
  decodeEntities: true,
};

function toCIdent(name) {
  return name.replace(/[^a-zA-Z0-9_]/g, "_").replace(/^(\d)/, "_$1");
}
function toPathKey(file) {
  return "/" + path.basename(file);
}
function chunkBytes(arr, per = 16) {
  const out = [];
  for (let i = 0; i < arr.length; i += per) {
    const slice = arr.slice(i, i + per).map(v => `0x${v.toString(16).padStart(2,"0")}`);
    out.push(slice.join(", "));
  }
  return out.join(",\n  ");
}

async function main() {
  console.log("--------------------------------------------------");
  console.log("OpenGarage HTML Compressor");
  console.log("--------------------------------------------------");

  // Partials (leading underscore: shared CSS, JS and images) are inlined into the
  // pages that include them, so every page stays a single self-contained request
  // (also through the OTC proxy). They are never emitted as assets of their own.
  const files = (await fs.readdir(WEB_DIR))
    .filter(f => f.endsWith(".html") && !f.startsWith("_"))
    .sort();

  async function expandIncludes(text, from, depth = 0) {
    if (depth > 4) throw new Error(`Include nesting too deep in ${from}`);
    const pattern = /<!--\s*#include\s+([A-Za-z0-9_.-]+)\s*-->/g;
    let out = "";
    let last = 0;
    for (const m of text.matchAll(pattern)) {
      if (!m[1].startsWith("_")) throw new Error(`${from}: only _partials can be included (${m[1]})`);
      const part = await fs.readFile(path.join(WEB_DIR, m[1]), "utf8");
      out += text.slice(last, m.index) + await expandIncludes(part, m[1], depth + 1);
      last = m.index + m[0].length;
    }
    return out + text.slice(last);
  }

  let totalOrig = 0;
  let totalGz   = 0;
  const items = [];
  let headerBody = `#pragma once
#include <Arduino.h>

typedef struct {
  const char* path;
  const uint8_t* data;
  const size_t len;
  const char* contentType;
  const char* contentEncoding;
} GzAsset;

`;

  for (const fname of files) {
    const inPath = path.join(WEB_DIR, fname);
    const expanded = await expandIncludes(await fs.readFile(inPath, "utf8"), fname);
    // Content ID: hash of the page source (before the ID is substituted). Used as the HTTP ETag,
    // and a page may embed its own ID through the __OG_PAGE_ID__ placeholder (the UI compares it with
    // the ID the device reports, to reload itself after a firmware update).
    const pageId = crypto.createHash("sha256").update(expanded).digest("hex").slice(0, 16);
    const raw = expanded.split("__OG_PAGE_ID__").join(pageId);
    const origSize = Buffer.byteLength(raw, "utf8");

    const minified = await minify(raw, MINIFY_OPTIONS);
    const gz = zlib.gzipSync(Buffer.from(minified, "utf8"), { level: 9 });
    const gzSize = gz.length;

    totalOrig += origSize;
    totalGz += gzSize;

    console.log(`Processing ${fname}...`);
    console.log(`  -> Original: ${origSize} bytes | Compressed: ${gzSize} bytes`);

    const ident = toCIdent(path.basename(fname, ".html")) + "_html_gz";
    const keyPath = toPathKey(fname);

    headerBody += `
// ${fname} (minified+gz, ${gz.length} bytes)
const uint8_t ${ident}[] PROGMEM = {
  ${chunkBytes([...gz])}
};
const size_t ${ident}_len = ${gz.length};
#define ${ident.toUpperCase()}_ID "${pageId}"

`;

    items.push({ path: keyPath, ident, lenIdent: `${ident}_len` });
  }

  // output asset
  /*headerBody += `\nstatic const GzAsset HTML_ASSETS[] PROGMEM = {\n`;
  for (const it of items) {
    headerBody += `  { "${it.path}", ${it.ident}, ${it.lenIdent}, "text/html; charset=utf-8", "gzip" },\n`;
  }
  headerBody += `};\n\n`;
  headerBody += `static const size_t HTML_ASSETS_COUNT = ${items.length};\n`;*/

  await fs.mkdir(path.dirname(OUT_H), { recursive: true });
  await fs.writeFile(OUT_H, headerBody, "utf8");

  console.log("\n--------------------------------------------------");
  console.log("Summary:");
  console.log(`Processed ${files.length} files.`);
  console.log(`Total original size: ${totalOrig} bytes`);
  console.log(`Total compressed size: ${totalGz} bytes`);
  const savings = totalOrig > 0 ? (100 * (1 - totalGz / totalOrig)).toFixed(2) : "0.00";
  console.log(`Total flash memory savings: ${savings}%`);
}

main().catch(err => {
  console.error(err);
  process.exit(1);
});

