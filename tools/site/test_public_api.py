"""Protect API coverage, overloads, scopes, literals and access boundaries."""
import json
import re
import unittest

from public_api import ROOT, parse_header, inventory, resolve_type, inherited_types, python_extensions


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


if __name__ == '__main__':
    unittest.main()
