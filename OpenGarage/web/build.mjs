import { gzip } from 'pako'
import fg from 'fast-glob'
import fs from 'fs/promises';
import { createWriteStream } from 'fs';
import { transform as transformCSS } from 'lightningcss';
import { minifySync as transformHTML } from '@swc/html';
import { minify as transformJS } from 'oxc-minify';

function minifyJS(code, filename) {
  const { code: out } = transformJS(filename, code, { minify: true });
  return out;
}

function minifyCSS(code, filename) {
  const { code: out } = transformCSS({
    filename,
    code: Buffer.from(code),
    minify: true,
  });
  return out.toString();
}

function processHTML(code, filename) {
  const scriptRegex = /<script\b[^>]*>([\s\S]*?)<\/script>/gi;
  const styleRegex = /<style\b[^>]*>([\s\S]*?)<\/style>/gi;

  let html = code;

  html = html.replace(scriptRegex, (match, content) => {
    const minified = minifyJS(content, filename);
    return `<script>${minified}</script>`;
  });

  html = html.replace(styleRegex, (match, content) => {
    const minified = minifyCSS(content, filename);
    return `<style>${minified}</style>`;
  });

  return transformHTML(html).code;
}

const files = await fg('public/*.{html,css,js}');

//TODO: The name is temp until the rest of the html files are created
const output = createWriteStream("../htmls2.h")

output.write("#ifndef HTML_H\n#define HTML_H\n")
for (const path of files) {
    const content = await fs.readFile(path, 'utf8');
    let minifiedContent;
    if (path.endsWith(".js")) {
        minifiedContent = minifyJS(content, path);
    } else if (path.endsWith(".css")) {
        minifiedContent = minifyCSS(content, path);
    } else if (path.endsWith(".html")) {
        minifiedContent = processHTML(content, path);
    } else {
        throw Error("Bad file: " + path);
    }
    const name = path.replace("public/", "").replace(".", "_");
    let compressed = gzip(minifiedContent);
    output.write(`const size_t ${name}_size = ${compressed.length};\n static const char ${name}[] PROGMEM = {`)
    compressed.forEach((byte, idx) => {
        if (idx > 0) {
            output.write(`, `);
        }

        output.write(`0x${byte.toString(16).padStart(2, '0')}`);
    });
    output.write("};\n");

}
output.write("#endif\n")
output.end();