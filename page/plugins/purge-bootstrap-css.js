// SPDX-FileCopyrightText: 2026 Hardkernel Co., Ltd.
// SPDX-License-Identifier: GPL-3.0-only

/**
 * Strips the rules the page never uses out of bootstrap.min.css (about three quarters
 * of it). Only Bootstrap's stylesheet is touched; xterm.css and src/style.css are
 * kept whole.
 *
 * A selector is kept when its class names appear anywhere in index.html or src/, or
 * in Bootstrap's own plugin sources, which add classes such as show, fade, active and
 * modal-backdrop at runtime. So a class has to appear literally in those files; one
 * assembled at runtime from pieces would be stripped.
 */

import {PurgeCSS} from 'purgecss';
import {dirname, join} from 'node:path';
import {createRequire} from 'node:module';

const require = createRequire(import.meta.url);
const bootstrapDir = dirname(require.resolve('bootstrap/package.json'));
const bootstrapCss = join(bootstrapDir, 'dist/css/bootstrap.min.css');

export default function purgeBootstrapCss() {
    let root;
    return {
        name: 'purge-bootstrap-css',
        enforce: 'pre',
        apply: 'build',
        configResolved(config) {
            root = config.root;
        },
        async transform(code, id) {
            if (id.split('?')[0] !== bootstrapCss) return null;
            const [result] = await new PurgeCSS().purge({
                content: [
                    join(root, 'index.html'),
                    join(root, 'src/**/*.{js,css}'),
                    join(bootstrapDir, 'js/src/**/*.js'),
                ],
                css: [{raw: code}],
                // The theme is switched at runtime by setting this attribute on <html>.
                safelist: {greedy: [/data-bs-theme/]},
            });
            return {code: result.css, map: null};
        },
    };
}
