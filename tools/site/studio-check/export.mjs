import { mkdirSync, writeFileSync } from 'node:fs';
import { join } from 'node:path';
import { STUDIO_TEMPLATES, generateCpp, generatePython, generateDesignerUi } from '../../../site/studio/studio-core.mjs';

const directory = process.argv[2];
mkdirSync(directory, { recursive: true });
for (const [id, { document }] of Object.entries(STUDIO_TEMPLATES)) {
  for (const [ext, generate] of Object.entries({ cpp: generateCpp, py: generatePython, ui: generateDesignerUi })) {
    writeFileSync(join(directory, `${id}.${ext}`), generate(document) + '\n');
  }
}
writeFileSync(join(directory, 'templates.json'), JSON.stringify(Object.fromEntries(
  Object.entries(STUDIO_TEMPLATES).map(([id, template]) => [id, template.document.root]))));
