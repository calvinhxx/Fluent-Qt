// Canonical Studio language keys are shared by the browser and the free AI Worker.
const languages = new Set(['en', 'zh', 'ja', 'ko', 'de', 'ru', 'es', 'fr']);

export function normalizeStudioLang(raw) {
  const tag = String(raw ?? '').trim().toLowerCase();
  if (/^zh-(?:hant|tw|hk|mo)(?:-|$)/.test(tag)) return 'zh-TW';
  const base = tag.split('-')[0];
  return languages.has(base) ? base : '';
}

export const quotaMessages = {
  quotaReached: { en: 'Daily free quota reached.', zh: '今日免费额度已用完。' },
  quotaHourly: { en: 'Hourly free quota reached for your IP.', zh: '当前 IP 已达到每小时免费额度。' },
  quotaMinute: { en: 'Too many requests per minute.', zh: '每分钟请求次数过多，请稍后重试。' },
  quotaLimited: { en: 'Free service request limit reached.', zh: '免费服务请求已达上限。' },
};
