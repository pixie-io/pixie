import commonjs from '@rollup/plugin-commonjs';
import peerDepsExternal from 'rollup-plugin-peer-deps-external';
import resolve from '@rollup/plugin-node-resolve';
import svgr from '@svgr/rollup';
import typescript from 'rollup-plugin-typescript2';
import url from '@rollup/plugin-url';

import pkg from './package.json';

export default {
  input: 'src/index.ts',
  output: [
    {
      file: pkg.main,
      format: 'cjs',
      exports: 'named',
      sourcemap: true,
    },
    {
      file: pkg.module,
      format: 'es',
      exports: 'named',
      sourcemap: true,
    },
  ],
  plugins: [
    peerDepsExternal(),
    url(),
    svgr(),
    resolve(),
    typescript({
      allowNonTsExtensions: true,
      useTsconfigDeclarationDir: true,
    }),
    commonjs(),
  ],
  onwarn(warning, warn) {
    // In >=99.99% of cases, calling 'eval' is not a wise decision. JSPB apparently ran into one of the exceptions.
    const isProtobufEvalWarning = warning.code === 'EVAL'
        && warning.loc && warning.loc && warning.loc.file.endsWith('google-protobuf.js');
    // Apollo's cache implementation has a polyfill for both `await` and `function*` at the top. Both of them check if
    // the `this` scope exists and already defined them. Rollup doesn't notice the safety check and warns. It's safe.
    const isApolloCacheUndefinedThisWarning = warning.code === 'THIS_IS_UNDEFINED'
        && warning.loc && warning.loc.file && warning.loc.file.includes('apollo3-cache-persist');
    if (!isProtobufEvalWarning && !isApolloCacheUndefinedThisWarning) {
      warn(warning);
    }
  },
};
