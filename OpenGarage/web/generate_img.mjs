import { optimize } from 'svgo';
import { glob } from 'glob'
import { readFileSync, writeFileSync } from 'fs';

export async function generateImg() {
    const svgs = await glob('image/*.svg');
    let output = "";

    svgs.forEach((path) => {
        const svgString = readFileSync(path);

        const result = optimize(svgString, {
        multipass: true,
        });

        const name = path.replace("image/", "").replace(".", "_");

        const optimizedSvgString = result.data;
        output += `export const ${name} = '${optimizedSvgString}';\n`;
    });

    writeFileSync("public/images.js", output);
}