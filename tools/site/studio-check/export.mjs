import { mkdirSync, writeFileSync } from 'node:fs';
import { join } from 'node:path';
import { STUDIO_TEMPLATES, generateCpp, generatePython, generateDesignerUi } from '../../../site/studio/studio-core.mjs';

const directory = process.argv[2];
mkdirSync(directory, { recursive: true });
const roots = {};

function exportPage(id, document) {
  for (const [ext, generate] of Object.entries({ cpp: generateCpp, py: generatePython, ui: generateDesignerUi })) {
    writeFileSync(join(directory, `${id}.${ext}`), generate(document) + '\n');
  }
  roots[id] = document.root;
}

for (const [id, { document }] of Object.entries(STUDIO_TEMPLATES)) exportPage(id, document);

// The C++ exporter places a stretch alongside the row's two direct widgets.
exportPage('aligned-row', {
  root: {
    id: 'root', type: 'HBox', props: { align: 'Center' },
    children: [
      { id: 'caption', type: 'Label', props: { text: 'Name' } },
      { id: 'actions', type: 'VBox', children: [{ id: 'apply', type: 'Button', props: { text: 'Apply' } }] },
    ],
  },
});
writeFileSync(join(directory, 'templates.json'), JSON.stringify(roots));
