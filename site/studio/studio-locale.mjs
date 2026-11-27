// Canonical Studio language keys are shared by the browser and the free AI Worker.
const languages = new Set(['en', 'zh', 'ja', 'ko', 'de', 'ru', 'es', 'fr']);

export function normalizeStudioLang(raw) {
  const tag = String(raw ?? '').trim().toLowerCase();
  if (/^zh-(?:hant|tw|hk|mo)(?:-|$)/.test(tag)) return 'zh-TW';
  const base = tag.split('-')[0];
  return languages.has(base) ? base : '';
}
