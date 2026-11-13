// SPDX-FileCopyrightText: 2025 Hardkernel Co., Ltd.
// SPDX-License-Identifier: GPL-3.0-only

import { fileURLToPath } from 'node:url';
import { defineConfig } from 'vite';
import { viteSingleFile } from 'vite-plugin-singlefile';
import viteCompression from 'vite-plugin-compression';
import bootstrapIconsSubset from './plugins/bootstrap-icons-subset.js';
import purgeBootstrapCss from './plugins/purge-bootstrap-css.js';

const local = (path) => fileURLToPath(new URL(path, import.meta.url));

// The firmware build (CMake) generates proto.js in its build directory and passes it
// in POWERMATE_PROTO_JS. `npm run dev` and `npm run build` generate src/proto.js.
const protoJs = process.env.POWERMATE_PROTO_JS;

export default defineConfig({
  plugins: [
    bootstrapIconsSubset(),
    purgeBootstrapCss(),
    viteSingleFile(),
    viteCompression(),
  ],
  resolve: {
    alias: {
      '@proto': protoJs ?? local('./src/proto.js'),
      // A proto.js outside page/ cannot find node_modules on its own.
      ...(protoJs && { 'protobufjs/minimal': local('./node_modules/protobufjs/minimal.js') }),
    },
  },
});
