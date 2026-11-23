// Static SVG artwork shared by document validation, AI import, the schematic and exports.
// This grammar accepts shapes and gradients. It has no stylesheets, external resources or active elements.
const elements = new Set('svg g defs path rect circle ellipse line polyline polygon linearGradient radialGradient stop clipPath text tspan title desc'.split(' '));
const attributes = new Set(('viewBox width height x y x1 y1 x2 y2 cx cy r rx ry fx fy d points id transform '
  + 'fill stroke color fill-rule clip-rule clip-path fill-opacity stroke-opacity opacity stroke-width '
  + 'stroke-linecap stroke-linejoin stroke-dasharray stroke-dashoffset gradientUnits gradientTransform '
  + 'offset stop-color stop-opacity preserveAspectRatio text-anchor font-size font-weight font-family').split(' '));
const paints = new Set(['fill', 'stroke', 'color', 'stop-color', 'clip-path']);
const paintValue = /^(?:[a-zA-Z]+|#[\da-fA-F]{3,8}|(?:rgb|hsl)a?\([\d.,% +\-]+\)|url\(#[\w.-]+\))$/;

export function isStudioPhotoUrl(value) {
  return typeof value === 'string' && value.length <= 512
    && /^https:\/\/images\.unsplash\.com\/photo-[a-zA-Z0-9_-]+(?:[?&#][^\s"'<>\\]*)?$/.test(value);
}

export function isSafeStudioSvg(value) {
  if (typeof value !== 'string' || value.length > 16384 || new TextEncoder().encode(value).length > 16384) return false;
  const xml = value.trim();
  const tag = /<(\/?)([A-Za-z]+)\b/y;
  const attr = /[ \t\r\n]+([\w:-]+)[ \t\r\n]*=[ \t\r\n]*(?:"([^"<>&]*)"|'([^'<>&]*)')/y;
  const end = /[ \t\r\n]*(\/?)>/y;
  const stack = [];
  let offset = 0;
  let rootSeen = false;
  while (offset < xml.length) {
    if (xml[offset] !== '<') {
      const next = xml.indexOf('<', offset);
      if (!stack.length || next < 0 || /[\u0000-\u0008\u000b\u000c\u000e-\u001f]|&(?!amp;|lt;|gt;|quot;|apos;)|\]\]>/.test(xml.slice(offset, next))) return false;
      offset = next;
    }
    tag.lastIndex = offset;
    const token = tag.exec(xml);
    if (!token || !elements.has(token[2])) return false;
    const [, closing, name] = token;
    if (!stack.length && (rootSeen || name !== 'svg')) return false;
    rootSeen = true;
    offset = tag.lastIndex;
    if (closing) {
      if (stack.pop() !== name) return false;
    } else {
      const seen = new Set();
      attr.lastIndex = offset;
      let match;
      while ((match = attr.exec(xml))) {
        const key = match[1], val = match[2] ?? match[3];
        if (seen.has(key)) return false;
        seen.add(key);
        if (key === 'xmlns') {
          if (val !== 'http://www.w3.org/2000/svg') return false;
        } else if (!attributes.has(key) || (paints.has(key)
          ? !paintValue.test(val) : !/^[\w.,%# +()\s-]*$/.test(val))) return false;
        offset = attr.lastIndex;
      }
    }
    end.lastIndex = offset;
    const terminator = end.exec(xml);
    if (!terminator || (closing && terminator[1])) return false;
    if (!closing && !terminator[1]) stack.push(name);
    offset = end.lastIndex;
  }
  return rootSeen && !stack.length;
}
