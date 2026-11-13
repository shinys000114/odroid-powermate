// SPDX-FileCopyrightText: 2026 Hardkernel Co., Ltd.
// SPDX-License-Identifier: GPL-3.0-only

/**
 * Serves `virtual:bootstrap-icons.css`: only the Bootstrap Icons the page uses, each
 * as an inline SVG mask, instead of the full icon font (two font files with ~2000
 * glyphs, which made up more than half of the gzipped page).
 *
 * Icons are found by scanning index.html and src/ for `bi-<name>` class names, so
 * markup keeps using `<i class="bi bi-gear">` as before. A class name has to appear
 * literally somewhere in those files; one assembled at runtime from pieces is not
 * found.
 */

import {readdirSync, readFileSync, existsSync} from 'node:fs';
import {join, dirname} from 'node:path';
import {createRequire} from 'node:module';

const VIRTUAL_ID = 'virtual:bootstrap-icons.css';
const ICON_CLASS = /\bbi-[a-z0-9]+(?:-[a-z0-9]+)*\b/g;

const require = createRequire(import.meta.url);
const iconsDir = join(dirname(require.resolve('bootstrap-icons/package.json')), 'icons');

// Same box as the font glyphs: 1em square, sitting .125em below the baseline, in the
// current text color. Unlike the font's rule this leaves out a bare `.bi`, which with
// no icon to mask would draw a filled square.
const BASE_RULE = `[class^="bi-"]::before,[class*=" bi-"]::before{content:"";display:inline-block;width:1em;height:1em;vertical-align:-.125em;background-color:currentColor;-webkit-mask:var(--bi) no-repeat center/contain;mask:var(--bi) no-repeat center/contain}`;

function sourceFiles(root) {
    const files = [join(root, 'index.html')];
    const walk = (dir) => {
        for (const entry of readdirSync(dir, {withFileTypes: true})) {
            const path = join(dir, entry.name);
            if (entry.isDirectory()) walk(path);
            else if (/\.(js|css|html)$/.test(entry.name)) files.push(path);
        }
    };
    walk(join(root, 'src'));
    return files;
}

function svgDataUri(svg) {
    const compact = svg
        .replace(/\s(width|height|fill|class)="[^"]*"/g, '')
        .replace(/>\s+</g, '><')
        .replace(/\s+/g, ' ')
        .trim()
        .replace(/"/g, "'");
    return `url("data:image/svg+xml,${compact.replace(/[%#<>\n]/g, encodeURIComponent)}")`;
}

export default function bootstrapIconsSubset() {
    let root;
    return {
        name: 'bootstrap-icons-subset',
        configResolved(config) {
            root = config.root;
        },
        resolveId(id) {
            return id === VIRTUAL_ID ? VIRTUAL_ID : null;
        },
        load(id) {
            if (id !== VIRTUAL_ID) return null;

            const names = new Set();
            for (const file of sourceFiles(root)) {
                this.addWatchFile(file);
                for (const match of readFileSync(file, 'utf8').matchAll(ICON_CLASS)) {
                    names.add(match[0].slice(3));
                }
            }

            const rules = [BASE_RULE];
            for (const name of [...names].sort()) {
                const svgPath = join(iconsDir, `${name}.svg`);
                if (!existsSync(svgPath)) {
                    this.error(`bi-${name} is used but bootstrap-icons has no icon named "${name}"`);
                }
                rules.push(`.bi-${name}{--bi:${svgDataUri(readFileSync(svgPath, 'utf8'))}}`);
            }
            return rules.join('\n');
        },
    };
}
