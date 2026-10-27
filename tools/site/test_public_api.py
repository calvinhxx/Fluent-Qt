"""Protect API coverage, overloads, scopes, literals and access boundaries."""
import json
import re
import unittest

from public_api import ROOT, documentation, parse_header, inventory, resolve_type, inherited_types, python_extensions


class PublicApiTest(unittest.TestCase):
    def test_public_members_and_bodies_are_separate(self):
        source = '''namespace fluent::demo {
class Example : public QObject {
    Q_OBJECT
    /** @brief A value. zh_CN: 数值。 */
    Q_PROPERTY(int value READ value WRITE setValue NOTIFY valueChanged)
public:
    enum class Mode { One = 1, Two = 1 << 2 };
    explicit Example(QObject* parent = nullptr);
    Example(int value, QObject* parent = nullptr);
    int value() const { return m_value; }
    void setValue(int value);
    Q_DECL_DEPRECATED_X("Use setValue") void oldSetter(int value);
    void setUrl(const QString& url = "https://example.test/two  words");
    struct Options { bool enabled = true; private: int hidden = 0; };
    using Factory = std::function<QWidget*(QObject*)>;
public slots:
    void reset();
signals:
    void valueChanged(int value);
protected:
    void paintEvent(QPaintEvent*) override;
private:
    void secret();
    int m_value = 0;
};
namespace detail { struct Hidden { int value; }; }
template <class T> T identity(T value) { return value; }
}'''
        types = parse_header(source)
        example = next(t for t in types if t['name'] == 'Example')
        members = example['members']
        names = {m['name'] for m in members}
        self.assertEqual(sum(m['kind'] == 'constructors' for m in members), 2)
        self.assertTrue({'value', 'setValue', 'valueChanged', 'Mode', 'Factory', 'Options', 'reset', 'oldSetter'} <= names)
        self.assertFalse({'secret', 'm_value'} & names)
        self.assertEqual(next(m for m in members if m['name'] == 'paintEvent')['access'], 'protected')
        self.assertEqual(next(m for m in members if m['kind'] == 'enums')['values'][1]['signature'], 'Two = 1 << 2')
        self.assertIn('https://example.test/two  words', next(m for m in members if m['name'] == 'setUrl')['signature'])
        self.assertNotIn('Hidden', {t['name'] for t in types})
        self.assertNotIn('hidden', {m['name'] for t in types for m in t['members']})
        self.assertIn('identity', {m['name'] for t in types for m in t['members']})
        self.assertEqual(next(m for m in members if m['name'] == 'oldSetter')['deprecated'], 'Use setValue')

    def test_every_installed_component_resolves_and_properties_are_covered(self):
        catalog = json.loads((ROOT / 'docs/ai/generated/fluentqt-ai-catalog.json').read_text())
        records = inventory()
        for component in catalog['components']:
            record = resolve_type(component['cpp']['qualified_type'])
            self.assertIsNotNone(record, component['id'])
            source = component['cpp']['declaration']
            properties = [m for t in records if t['source'] == source for m in t['members'] if m['kind'] == 'properties']
            count = len(re.findall(r'\bQ_PROPERTY\(', (ROOT / source).read_text()))
            self.assertEqual(len(properties), count, source)
        self.assertFalse(any(m['access'] == 'private' for t in records for m in t['members']))

    def test_button_api_includes_real_overloads_signals_enums_and_mixins(self):
        button = resolve_type('fluent::basicinput::Button')
        self.assertEqual(sum(m['kind'] == 'constructors' for m in button['members']), 2)
        self.assertIn('fluentStyleChanged', {m['name'] for m in button['members'] if m['kind'] == 'signals'})
        self.assertIn('ButtonStyle', {m['name'] for m in button['members'] if m['kind'] == 'enums'})
        inherited = {m['name'] for b in inherited_types(button) for m in b['members']}
        self.assertTrue({'setTheme', 'anchors', 'setState'} <= inherited)

    def test_python_ownership_extensions_come_from_python_source(self):
        additions = python_extensions('fluentqt.layout', 'Field')
        by_name = {m['name']: m for m in additions}
        self.assertIn('setBorrowedEditor', by_name)
        self.assertEqual(by_name['setBorrowedEditor']['signature'], 'setBorrowedEditor(widget)')
        self.assertTrue(by_name['setBorrowedEditor']['source'].endswith('layout.py'))

    def test_trailing_comments_and_multiparagraph_zh_do_not_leak(self):
        flyout = resolve_type('fluent::dialogs_flyouts::Flyout')
        placement = next(m for m in flyout['members'] if m['name'] == 'Placement')
        values = {v['name']: v['description'] for v in placement['values']}
        self.assertEqual(values['Top'], {'en': 'Above the anchor.', 'zh': '在 anchor 上方。'})
        self.assertEqual(values['Bottom'], {'en': 'Below the anchor.', 'zh': '在 anchor 下方。'})
        self.assertEqual(values['Auto'], {'en': 'Prefer Bottom, flipping to Top when space runs out.', 'zh': '优先 Bottom，空间不足时反转到 Top。'})
        breakpoints = resolve_type('Breakpoints')
        constants = {m['name']: m['description'] for m in breakpoints['members'] if m['kind'] == 'constants'}
        self.assertEqual(constants['Small']['zh'], '手机或窄窗口紧凑布局。')
        self.assertEqual(constants['Medium']['zh'], '平板或半屏中等布局。')
        self.assertEqual(constants['Large']['zh'], '桌面展开布局。')
        chart_data = resolve_type('fluent::charts::ChartData')
        self.assertNotIn('zh_CN:', chart_data['description']['zh'])
        self.assertNotIn('X must be finite', chart_data['description']['zh'])

    def test_zh_keeps_ascii_lines_between_chinese_lines(self):
        encodings = documentation(['/** @brief Supported encodings.\n * zh_CN: 支持以下编码：\n'
                                   ' * UTF-8 / UTF-16\n * 其他编码会返回错误。\n */'])
        self.assertEqual(encodings, {'en': 'Supported encodings.', 'zh': '支持以下编码： UTF-8 / UTF-16 其他编码会返回错误。'})
        details = documentation(['/** @brief Mouse coordinates.\n * zh_CN: 鼠标坐标。\n * Qt 6 returns QPointF.\n */'])
        self.assertEqual(details['zh'], '鼠标坐标。')

    def test_trailing_comments_yield_to_bilingual_briefs_and_never_leak(self):
        source = '''class Example : public QObject {
public: // Helpers for child widgets.
    // Updates the hue. zh_CN: 更新色相。
    void setHue(qreal h);  // 0.0 - 1.0
    // --- Flow ---
    int flow() const;  // Current flow direction.
    /// Leading Doxygen brief.
    void explicitDoc();  ///< Trailing Doxygen brief.
    void plain();
};'''
        members = {m['name']: m['description'] for m in parse_header(source)[0]['members']}
        self.assertEqual(members['setHue'], {'en': 'Updates the hue.', 'zh': '更新色相。'})
        self.assertEqual(members['flow']['en'], 'Current flow direction.')
        self.assertEqual(members['explicitDoc']['en'], 'Trailing Doxygen brief.')
        self.assertEqual(members['plain'], {'en': '', 'zh': ''})


if __name__ == '__main__':
    unittest.main()
