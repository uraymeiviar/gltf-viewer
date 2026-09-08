import resolve from '@rollup/plugin-node-resolve';
import commonjs from '@rollup/plugin-commonjs';
import typescript from '@rollup/plugin-typescript';


export default {
    input: 'src/thirdparty.ts',
    inlineDynamicImports: true,
    output: {
        file: '../qml/js/thirdparty.qml.mjs',
        format: 'esm',
        esModule: true,
        // intro: 'const exports = {};',  // A hack for QML environment
    },
    plugins: [
        resolve(),
        commonjs(),
        typescript()
    ]
};
