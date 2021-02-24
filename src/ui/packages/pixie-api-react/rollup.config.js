import commonjs from '@rollup/plugin-commonjs';
import peerDepsExternal from 'rollup-plugin-peer-deps-external';
import resolve from '@rollup/plugin-node-resolve';
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
        && warning.loc && warning.loc && warning.loc.file.includes('pixie-api');
    if (!isProtobufEvalWarning) {
      warn(warning);
    }
  },
};
