import esbuild from 'esbuild';
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const __dirname = path.dirname(fileURLToPath(import.meta.url));
const entryPoint = path.resolve(__dirname, 'entry.mjs');
const outfile = path.resolve(__dirname, '../../../site/studio/studio-ai-sdk.mjs');

const stubGateway = {
  name: 'stub-gateway',
  setup(build) {
    build.onResolve({ filter: /^@ai-sdk\/gateway$/ }, () => ({
      path: 'stub-gateway',
      namespace: 'stub',
    }));
    build.onLoad({ filter: /.*/, namespace: 'stub' }, () => ({
      contents: [
        'export class GatewayError extends Error { static isInstance() { return false; } }',
        'export class GatewayAuthenticationError extends GatewayError { static isInstance() { return false; } }',
        'export const gateway = undefined;',
        'export const gatewayProvider = undefined;',
        'export function createGateway() { throw new Error("Gateway disabled in browser Studio bundle"); }',
      ].join('\n'),
      loader: 'js',
    }));
  },
};

await esbuild.build({
  entryPoints: [entryPoint],
  outfile,
  bundle: true,
  format: 'esm',
  platform: 'browser',
  target: ['es2022'],
  minify: true,
  treeShaking: true,
  plugins: [stubGateway],
  define: {
    'process.env.NODE_ENV': '"production"',
  },
  banner: {
    js: '// Bundled via tools/site/studio-ai-sdk/build.mjs (Vercel AI SDK)',
  },
});

const bytes = fs.statSync(outfile).size;
console.log(`Built ${path.relative(process.cwd(), outfile)}: ${bytes} bytes (${(bytes / 1024).toFixed(1)} KB)`);
if (bytes >= 900_000) {
  throw new Error(`Bundled SDK exceeds 900,000-byte limit: ${bytes} bytes`);
}
