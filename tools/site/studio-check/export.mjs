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
// A plain child is valid in Studio; native Accordion owns Expander sections.
exportPage('accordion-pickers', {
  root: { id: 'root', type: 'VBox', children: [
    { id: 'sections', type: 'Accordion', children: [
      { id: 'caption', type: 'Label', props: { text: 'Release Notes' } },
      { id: 'caption_section', type: 'Expander', props: { headerText: 'Details', expanded: true },
        children: [{ id: 'details', type: 'Label', props: { text: 'Version' } }] },
    ] },
    { id: 'empty_date', type: 'DatePicker' }, { id: 'empty_time', type: 'TimePicker' },
    { id: 'chosen_date', type: 'DatePicker', props: { date: '2031-03-14' } },
    { id: 'chosen_time', type: 'TimePicker', props: { time: '16:12' } },
  ] },
});
exportPage('zoned-range', {
  root: { id: 'root', type: 'VBox', children: [
    { id: 'range', type: 'DateTimeRangePicker', props: {
      startDateTime: '2031-03-14T22:10:00+05:30', endDateTime: '2031-03-15T10:20:00Z',
    } },
  ] },
});
writeFileSync(join(directory, 'templates.json'), JSON.stringify(roots));
