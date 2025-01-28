import { gzip } from 'pako'
import { glob } from 'glob'
import { readFileSync, createWriteStream } from 'fs';

const files = await glob('public/*.{html,css,js}');
//TODO: The name is temp until the rest of the html files are created
const output = createWriteStream("../htmls2.h")

output.write("#ifndef HTML_H\n#define HTML_H\n")
files.forEach((path) => {
    const content = readFileSync(path);

    
    const name = path.replace("public/", "").replace(".", "_");
    let compressed = gzip(content);
    output.write(`const size_t ${name}_size = ${compressed.length};\n static const char ${name}[] PROGMEM = {`)
    compressed.forEach((byte, idx) => {
        if (idx > 0) {
            output.write(`, `);
        }

        output.write(`0x${byte.toString(16).padStart(2, '0')}`);
    });
    output.write("};\n");

});
output.write("#endif\n")
output.end();