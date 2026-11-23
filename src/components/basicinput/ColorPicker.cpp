#include "compatibility/private/WidgetCursorCompat_p.h"
#include "compatibility/PaintCompat.h"
#include "ColorPicker.h"

#include "components/textfields/Label.h"
#include "components/textfields/LineEdit.h"
#include "components/basicinput/Slider.h"
#include "components/basicinput/private/ColorPickerAccessibility_p.h"
#include <QIntValidator>
#include <QRegularExpressionValidator>
#include <QSlider>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>

namespace fluent::basicinput {

class ColorPickSurface : public QWidget {
public:
    ColorPickSurface(QWidget* parent, Qt::CursorShape idle) : QWidget(parent)
    {
        setMouseTracking(true);
        setCursor(idle);
        setFocusPolicy(Qt::StrongFocus);
    }

protected:
    bool event(QEvent* event) override
    {
        if (::compatibility::detail::interruptsPointerGesture(event->type())
            || (event->type() == QEvent::EnabledChange && !isEnabled())) {
            finishPicking();
        }
        return QWidget::event(event);
    }

    void mousePressEvent(QMouseEvent* event) override
    {
        if (event->button() != Qt::LeftButton) {
            QWidget::mousePressEvent(event);
            return;
        }
        m_picking = true;
        m_pointerCursor.apply(Qt::BlankCursor);
        handlePos(event->pos());
    }
    void mouseMoveEvent(QMouseEvent* event) override
    {
        if (m_picking && event->buttons().testFlag(Qt::LeftButton))
            handlePos(event->pos());
    }
    void mouseReleaseEvent(QMouseEvent* event) override
    {
        if (event->button() == Qt::LeftButton)
            finishPicking();
    }
    void keyPressEvent(QKeyEvent* event) override
    {
        if (m_picking && event->key() == Qt::Key_Escape) {
            finishPicking();
            event->accept();
            return;
        }
        QWidget::keyPressEvent(event);
    }

private:
    virtual void handlePos(const QPoint& position) = 0;
    void finishPicking()
    {
        m_picking = false;
        m_pointerCursor.clear();
    }

    ::compatibility::detail::ScopedWidgetCursor m_pointerCursor{this};
    bool m_picking = false;
};

class ColorSpectrumWidget : public ColorPickSurface {
public:
    explicit ColorSpectrumWidget(ColorPicker* picker, QWidget* parent = nullptr)
        : ColorPickSurface(parent, Qt::CrossCursor), m_picker(picker)
    {}

protected:
    void paintEvent(QPaintEvent*) override
    {
        if (!m_picker)
            return;
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        QRectF r = rect().adjusted(1, 1, -1, -1);

        qreal radius = m_picker->themeRadius().control;
        QPainterPath clipPath;
        clipPath.addRoundedRect(r, radius, radius);
        p.setClipPath(clipPath);

        // Horizontal saturation (white -> hue) overlaid with vertical brightness (transparent -> black).
        QColor hueColor = QColor::fromHsvF(m_picker->hue(), 1.0, 1.0);
        QLinearGradient gradH(r.topLeft(), r.topRight());
        gradH.setColorAt(0.0, Qt::white);
        gradH.setColorAt(1.0, hueColor);
        p.fillRect(r, gradH);

        QLinearGradient gradV(r.topLeft(), r.bottomLeft());
        gradV.setColorAt(0.0, QColor(0, 0, 0, 0));
        gradV.setColorAt(1.0, QColor(0, 0, 0, 255));
        p.fillRect(r, gradV);

        qreal sx = m_picker->saturation();
        qreal sv = 1.0 - m_picker->value();
        QPointF pos(r.left() + sx * r.width(), r.top() + sv * r.height());
        p.setPen(QPen(Qt::white, 2));
        p.setBrush(Qt::NoBrush);
        p.drawEllipse(pos, 6, 6);
    }

    void keyPressEvent(QKeyEvent* event) override
    {
        if (!m_picker) {
            ColorPickSurface::keyPressEvent(event);
            return;
        }
        const qreal step = event->modifiers().testFlag(Qt::ShiftModifier) ? 0.05 : 0.01;
        qreal saturation = m_picker->saturation();
        qreal value = m_picker->value();
        switch (event->key()) {
        case Qt::Key_Left:
            saturation -= step;
            break;
        case Qt::Key_Right:
            saturation += step;
            break;
        case Qt::Key_Up:
            value += step;
            break;
        case Qt::Key_Down:
            value -= step;
            break;
        default:
            ColorPickSurface::keyPressEvent(event);
            return;
        }
        m_picker->setSVFromSpectrum(qBound(0.0, saturation, 1.0), qBound(0.0, value, 1.0));
        event->accept();
    }

private:
    void handlePos(const QPoint& pnt) override
    {
        if (!m_picker)
            return;
        QRectF r = rect().adjusted(1, 1, -1, -1);
        qreal sx = (pnt.x() - r.left()) / r.width();
        qreal sv = (pnt.y() - r.top()) / r.height();
        sx = std::clamp(sx, 0.0, 1.0);
        sv = std::clamp(sv, 0.0, 1.0);
        m_picker->setSVFromSpectrum(sx, 1.0 - sv);
        update();
    }

    ColorPicker* m_picker = nullptr;
};

class HueBarWidget : public ColorPickSurface {
public:
    explicit HueBarWidget(ColorPicker* picker, QWidget* parent = nullptr)
        : ColorPickSurface(parent, Qt::ArrowCursor), m_picker(picker)
    {}

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        QRectF r = rect().adjusted(2, 2, -2, -2);

        QLinearGradient grad(r.topLeft(), r.bottomLeft());
        grad.setColorAt(0.0, QColor::fromHsvF(0.0f, 1.0f, 1.0f));
        grad.setColorAt(1.0 / 6.0, QColor::fromHsvF(1.0f / 6.0f, 1.0f, 1.0f));
        grad.setColorAt(2.0 / 6.0, QColor::fromHsvF(2.0f / 6.0f, 1.0f, 1.0f));
        grad.setColorAt(3.0 / 6.0, QColor::fromHsvF(3.0f / 6.0f, 1.0f, 1.0f));
        grad.setColorAt(4.0 / 6.0, QColor::fromHsvF(4.0f / 6.0f, 1.0f, 1.0f));
        grad.setColorAt(5.0 / 6.0, QColor::fromHsvF(5.0f / 6.0f, 1.0f, 1.0f));
        grad.setColorAt(1.0, QColor::fromHsvF(1.0f, 1.0f, 1.0f));

        p.fillRect(r, grad);

        if (m_picker) {
            qreal h = m_picker->hue();
            qreal y = r.top() + h * r.height();
            p.setPen(QPen(Qt::white, 2));
            p.drawLine(QPointF(r.left(), y), QPointF(r.right(), y));
        }
    }

    void keyPressEvent(QKeyEvent* event) override
    {
        if (!m_picker) {
            ColorPickSurface::keyPressEvent(event);
            return;
        }
        const qreal step = event->modifiers().testFlag(Qt::ShiftModifier) ? 10.0 / 359.0 : 1.0 / 359.0;
        qreal hue = m_picker->hue();
        switch (event->key()) {
        case Qt::Key_Left:
        case Qt::Key_Up:
            hue -= step;
            break;
        case Qt::Key_Right:
        case Qt::Key_Down:
            hue += step;
            break;
        case Qt::Key_Home:
            hue = 0.0;
            break;
        case Qt::Key_End:
            hue = 1.0;
            break;
        default:
            ColorPickSurface::keyPressEvent(event);
            return;
        }
        while (hue < 0.0)
            hue += 1.0;
        while (hue > 1.0)
            hue -= 1.0;
        m_picker->setHueFromBar(hue);
        event->accept();
    }

private:
    void handlePos(const QPoint& pnt) override
    {
        if (!m_picker)
            return;
        QRectF r = rect().adjusted(2, 2, -2, -2);
        qreal h = (pnt.y() - r.top()) / r.height();
        h = std::clamp(h, 0.0, 1.0);
        m_picker->setHueFromBar(h);
        update();
    }

    ColorPicker* m_picker = nullptr;
};

class ColorPreviewPaneWidget : public QWidget {
public:
    explicit ColorPreviewPaneWidget(ColorPicker* picker, QWidget* parent = nullptr) : QWidget(parent), m_picker(picker)
    {
        setFixedWidth(28);
        setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        if (!m_picker)
            return;
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        QRectF r = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
        const int tile = 6;
        for (int y = 0; y < height(); y += tile) {
            for (int x = 0; x < width(); x += tile) {
                bool light = ((x / tile) + (y / tile)) % 2 == 0;
                p.fillRect(QRect(x, y, tile, tile), light ? Qt::white : QColor(0xE0, 0xE0, 0xE0));
            }
        }
        p.fillRect(r, m_picker->color());
        p.setPen(QPen(QColor(0, 0, 0, 60), 1));
        p.setBrush(Qt::NoBrush);
        p.drawRect(r);
    }

private:
    ColorPicker* m_picker = nullptr;
};

ColorPicker::ColorPicker(QWidget* parent) : QWidget(parent)
{
    detail::ensureColorPickerAccessibilityFactory();
    initUi();
    setColor(QColor(255, 255, 255, 255));
}

void ColorPicker::initUi()
{
    using Edge = ::fluent::AnchorLayout::Edge;
    auto* anchorLayout = new ::fluent::AnchorLayout(this);
    const auto& spacing = themeSpacing();
    const int pad = spacing.padding.card;
    const int gap = spacing.gap.normal;
    const auto configureFieldLabel = [](fluent::textfields::Label* label) {
        // Use the label's own color role stylesheet so styled containers cannot override WindowText.
        label->setTextColorRole(fluent::textfields::Label::TextColorRole::Primary);
        label->setFixedWidth(48);
    };

    auto* inputsPanel = new QWidget(this);
    inputsPanel->setObjectName(QStringLiteral("ColorPicker.InputsPanel"));
    auto* inputsLayout = new QVBoxLayout(inputsPanel);
    inputsLayout->setContentsMargins(0, 0, 0, 0);
    inputsLayout->setSpacing(spacing.gap.tight);

    auto createTextRow =
        [&](const QString& labelText, fluent::textfields::LineEdit*& edit, void (ColorPicker::*slot)()) {
            auto* row = new QHBoxLayout();
            row->setContentsMargins(0, 0, 0, 0);
            row->setSpacing(spacing.gap.tight);
            auto* label = new fluent::textfields::Label(labelText, inputsPanel);
            configureFieldLabel(label);
            row->addWidget(label);
            edit = new fluent::textfields::LineEdit(inputsPanel);
            edit->setAccessibleName(labelText.left(labelText.size() - 1));
            edit->setClearButtonEnabled(false);
            row->addWidget(edit, 1);
            connect(edit, &QLineEdit::returnPressed, this, slot);
            inputsLayout->addLayout(row);
        };
    createTextRow(tr("Hex:"), m_hexEdit, &ColorPicker::handleHexEdited);
    createTextRow(tr("Red:"), m_rEdit, &ColorPicker::handleChannelEdited);
    createTextRow(tr("Green:"), m_gEdit, &ColorPicker::handleChannelEdited);
    createTextRow(tr("Blue:"), m_bEdit, &ColorPicker::handleChannelEdited);
    m_alphaInputRowWidget = new QWidget(inputsPanel);
    auto* alphaInputRow = new QHBoxLayout(m_alphaInputRowWidget);
    alphaInputRow->setContentsMargins(0, 0, 0, 0);
    alphaInputRow->setSpacing(spacing.gap.tight);
    auto* alphaInputLabel = new fluent::textfields::Label(tr("Alpha:"), m_alphaInputRowWidget);
    configureFieldLabel(alphaInputLabel);
    m_aEdit = new fluent::textfields::LineEdit(m_alphaInputRowWidget);
    m_aEdit->setAccessibleName(tr("Alpha"));
    m_aEdit->setClearButtonEnabled(false);
    alphaInputRow->addWidget(alphaInputLabel);
    alphaInputRow->addWidget(m_aEdit, 1);
    connect(m_aEdit, &QLineEdit::returnPressed, this, &ColorPicker::handleChannelEdited);
    inputsLayout->addWidget(m_alphaInputRowWidget);

    m_hexEdit->setValidator(new QRegularExpressionValidator(QRegularExpression("^#?[0-9A-Fa-f]{0,8}$"), m_hexEdit));
    m_rEdit->setValidator(new QIntValidator(0, 255, m_rEdit));
    m_gEdit->setValidator(new QIntValidator(0, 255, m_gEdit));
    m_bEdit->setValidator(new QIntValidator(0, 255, m_bEdit));
    m_aEdit->setValidator(new QIntValidator(0, 255, m_aEdit));

    ::fluent::AnchorLayout::Anchors aInputs;
    aInputs.left = {this, Edge::Left, pad};
    aInputs.right = {this, Edge::Right, -pad};
    aInputs.bottom = {this, Edge::Bottom, -pad};
    anchorLayout->addAnchoredWidget(inputsPanel, aInputs);

    auto* slidersPanel = new QWidget(this);
    slidersPanel->setObjectName(QStringLiteral("ColorPicker.SlidersPanel"));
    auto* slidersLayout = new QVBoxLayout(slidersPanel);
    slidersLayout->setContentsMargins(0, 0, 0, 0);
    slidersLayout->setSpacing(spacing.gap.tight);

    auto* valueRow = new QHBoxLayout();
    valueRow->setContentsMargins(0, 0, 0, 0);
    valueRow->setSpacing(spacing.gap.tight);
    auto* valueLabel = new fluent::textfields::Label(tr("Value:"), slidersPanel);
    configureFieldLabel(valueLabel);
    m_valueSlider = new Slider(Qt::Horizontal, slidersPanel);
    m_valueSlider->setAccessibleName(tr("Brightness"));
    m_valueSlider->setMinimum(0);
    m_valueSlider->setMaximum(100);
    m_valueSlider->setValue(100);
    connect(m_valueSlider, &QSlider::valueChanged, this, &ColorPicker::setValueFromSlider);
    valueRow->addWidget(valueLabel);
    valueRow->addWidget(m_valueSlider, 1);
    slidersLayout->addLayout(valueRow);

    m_alphaRowWidget = new QWidget(slidersPanel);
    auto* alphaRow = new QHBoxLayout(m_alphaRowWidget);
    alphaRow->setContentsMargins(0, 0, 0, 0);
    alphaRow->setSpacing(spacing.gap.tight);
    auto* alphaLabel = new fluent::textfields::Label(tr("Alpha:"), m_alphaRowWidget);
    configureFieldLabel(alphaLabel);
    m_alphaSlider = new Slider(Qt::Horizontal, m_alphaRowWidget);
    m_alphaSlider->setAccessibleName(tr("Alpha"));
    m_alphaSlider->setMinimum(0);
    m_alphaSlider->setMaximum(255);
    m_alphaSlider->setValue(255);
    connect(m_alphaSlider, &QSlider::valueChanged, this, &ColorPicker::setAlphaFromSlider);
    alphaRow->addWidget(alphaLabel);
    alphaRow->addWidget(m_alphaSlider, 1);
    slidersLayout->addWidget(m_alphaRowWidget);

    ::fluent::AnchorLayout::Anchors aSliders;
    aSliders.left = {this, Edge::Left, pad};
    aSliders.right = {this, Edge::Right, -pad};
    aSliders.bottom = {inputsPanel, Edge::Top, -gap};
    anchorLayout->addAnchoredWidget(slidersPanel, aSliders);

    m_spectrum = new ColorSpectrumWidget(this, this);
    m_spectrum->setObjectName(QStringLiteral("ColorPicker.Spectrum"));
    m_spectrum->setMinimumSize(200, 160);
    m_spectrum->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    m_hueBar = new HueBarWidget(this, this);
    m_hueBar->setObjectName(QStringLiteral("ColorPicker.HueBar"));
    m_hueBar->setFixedWidth(24);

    m_previewPane = new ColorPreviewPaneWidget(this, this);
    m_previewPane->setObjectName(QStringLiteral("ColorPicker.PreviewPane"));

    ::fluent::AnchorLayout::Anchors aSpectrum;
    aSpectrum.left = {this, Edge::Left, pad};
    aSpectrum.top = {this, Edge::Top, pad};
    aSpectrum.right = {m_hueBar, Edge::Left, -gap};
    aSpectrum.bottom = {slidersPanel, Edge::Top, -gap};
    anchorLayout->addAnchoredWidget(m_spectrum, aSpectrum);

    ::fluent::AnchorLayout::Anchors aHue;
    aHue.right = {m_previewPane, Edge::Left, -gap};
    aHue.top = {this, Edge::Top, pad};
    aHue.bottom = {slidersPanel, Edge::Top, -gap};
    anchorLayout->addAnchoredWidget(m_hueBar, aHue);

    ::fluent::AnchorLayout::Anchors aPreview;
    aPreview.right = {this, Edge::Right, -pad};
    aPreview.top = {this, Edge::Top, pad};
    aPreview.bottom = {slidersPanel, Edge::Top, -gap};
    anchorLayout->addAnchoredWidget(m_previewPane, aPreview);

    onThemeUpdated();
}

void ColorPicker::onThemeUpdated()
{
    const auto& fs = themeFont(Typography::FontRole::Body);
    QFont f = fs.toQFont();

    // Refresh theme-aware children that may not pick up parent font updates on their own.
    const auto labels = findChildren<fluent::textfields::Label*>();
    for (auto* label : labels) {
        label->onThemeUpdated();
    }

    setFont(f);

    if (m_hexEdit)
        m_hexEdit->onThemeUpdated();
    if (m_rEdit)
        m_rEdit->onThemeUpdated();
    if (m_gEdit)
        m_gEdit->onThemeUpdated();
    if (m_bEdit)
        m_bEdit->onThemeUpdated();
    if (m_aEdit)
        m_aEdit->onThemeUpdated();
    if (m_valueSlider)
        m_valueSlider->onThemeUpdated();
    if (m_alphaSlider)
        m_alphaSlider->onThemeUpdated();

    // Spectrum, hue bar, and preview pane are plain QWidget painters rather than FluentElements.
    if (m_spectrum)
        m_spectrum->update();
    if (m_hueBar)
        m_hueBar->update();
    if (m_previewPane)
        m_previewPane->update();

    updateFromColor();
    update();
}

void ColorPicker::setColor(const QColor& c)
{
    if (c == m_color)
        return;
    const qreal oldHue = m_h;
    const qreal oldSaturation = m_s;
    const qreal oldValue = m_v;
    m_color = c;

    FluentColorComponent h, s, v, a;
    c.getHsvF(&h, &s, &v, &a);

    // Keep prior hue for grayscale (h < 0) and prior saturation for black (v == 0) so dragging doesn't snap handles.
    if (h < 0.0f) {
        h = m_h;
    }

    if (qFuzzyIsNull(v)) {
        s = m_s;
    }

    m_h = h;
    m_s = s;
    m_v = v;

    updateFromColor();
    detail::notifyColorPickerValueChanged(this);
    if (!qFuzzyCompare(oldHue + 1.0, m_h + 1.0))
        detail::notifyColorPickerHueChanged(m_hueBar);
    if (!qFuzzyCompare(oldSaturation + 1.0, m_s + 1.0) || !qFuzzyCompare(oldValue + 1.0, m_v + 1.0)) {
        detail::notifyColorPickerSpectrumChanged(m_spectrum);
    }
    emit colorChanged(m_color);
}

void ColorPicker::setAlphaEnabled(bool enabled)
{
    if (m_alphaEnabled == enabled)
        return;
    m_alphaEnabled = enabled;
    if (m_alphaRowWidget)
        m_alphaRowWidget->setVisible(enabled);
    if (m_alphaInputRowWidget)
        m_alphaInputRowWidget->setVisible(enabled);
    updateFromColor();
    detail::notifyColorPickerValueChanged(this);
    detail::notifyColorPickerStructureChanged(this);
    emit alphaEnabledChanged(enabled);
}

void ColorPicker::setHueFromBar(qreal h)
{
    h = qBound(0.0, h, 1.0);
    const qreal oldHue = m_h;
    m_h = h;
    QColor c = QColor::fromHsvF(m_h, m_s, m_v, m_color.alphaF());
    setColor(c);
    if (!qFuzzyCompare(oldHue + 1.0, m_h + 1.0))
        detail::notifyColorPickerHueChanged(m_hueBar);
    if (m_spectrum)
        m_spectrum->update();
}

void ColorPicker::setSVFromSpectrum(qreal s, qreal v)
{
    s = qBound(0.0, s, 1.0);
    v = qBound(0.0, v, 1.0);
    const qreal oldSaturation = m_s;
    const qreal oldValue = m_v;
    m_s = s;
    m_v = v;
    QColor c = QColor::fromHsvF(m_h, m_s, m_v, m_color.alphaF());
    setColor(c);
    if (!qFuzzyCompare(oldSaturation + 1.0, m_s + 1.0) || !qFuzzyCompare(oldValue + 1.0, m_v + 1.0)) {
        detail::notifyColorPickerSpectrumChanged(m_spectrum);
    }
}

void ColorPicker::setValueFromSlider(int percent)
{
    if (m_isInternalUpdate)
        return;
    const qreal oldValue = m_v;
    m_v = std::clamp(percent / 100.0, 0.0, 1.0);
    QColor c = QColor::fromHsvF(m_h, m_s, m_v, m_color.alphaF());
    setColor(c);
    if (!qFuzzyCompare(oldValue + 1.0, m_v + 1.0))
        detail::notifyColorPickerSpectrumChanged(m_spectrum);
}

void ColorPicker::setAlphaFromSlider(int alpha)
{
    if (m_isInternalUpdate)
        return;
    alpha = std::clamp(alpha, 0, 255);
    QColor c = m_color;
    c.setAlpha(alpha);
    setColor(c);
}

void ColorPicker::handleChannelEdited()
{
    if (m_isInternalUpdate)
        return;

    if (!m_rEdit || !m_gEdit || !m_bEdit || !m_aEdit)
        return;

    int r = m_rEdit->text().toInt();
    int g = m_gEdit->text().toInt();
    int b = m_bEdit->text().toInt();
    int a = m_alphaEnabled ? m_aEdit->text().toInt() : 255;

    // QIntValidator allows intermediate strings outside [0, 255]; clamp explicitly.
    r = std::clamp(r, 0, 255);
    g = std::clamp(g, 0, 255);
    b = std::clamp(b, 0, 255);
    a = std::clamp(a, 0, 255);

    QColor c(r, g, b, a);
    setColor(c);
}

void ColorPicker::handleHexEdited()
{
    if (m_isInternalUpdate)
        return;

    bool ok = false;
    QColor c = hexToColor(m_hexEdit->text().trimmed(), &ok);
    if (ok) {
        setColor(c);
    } else {
        updateFromColor();
    }
}

void ColorPicker::updateFromColor()
{
    m_isInternalUpdate = true;
    if (m_rEdit && m_gEdit && m_bEdit && m_aEdit) {
        m_rEdit->setText(QString::number(m_color.red()));
        m_gEdit->setText(QString::number(m_color.green()));
        m_bEdit->setText(QString::number(m_color.blue()));
        m_aEdit->setText(QString::number(m_color.alpha()));
    }
    if (m_hexEdit)
        m_hexEdit->setText(colorToHex(m_color, m_alphaEnabled));
    if (m_valueSlider)
        m_valueSlider->setValue(qRound(m_v * 100));
    if (m_alphaSlider)
        m_alphaSlider->setValue(m_color.alpha());
    if (m_spectrum)
        m_spectrum->update();
    if (m_hueBar)
        m_hueBar->update();
    if (m_previewPane)
        m_previewPane->update();
    m_isInternalUpdate = false;
}

QString ColorPicker::colorToHex(const QColor& c, bool withAlpha) const
{
    QString hex =
        QString("#%1%2%3")
            .arg(c.red(), 2, 16, QLatin1Char('0'))
            .arg(c.green(), 2, 16, QLatin1Char('0'))
            .arg(c.blue(), 2, 16, QLatin1Char('0'))
            .toUpper();
    if (withAlpha) {
        hex += QString("%1").arg(c.alpha(), 2, 16, QLatin1Char('0')).toUpper();
    }
    return hex;
}

QColor ColorPicker::hexToColor(const QString& text, bool* ok) const
{
    QString t = text;
    if (t.startsWith('#'))
        t.remove(0, 1);
    bool localOk = false;
    int len = t.length();
    if (len != 6 && len != 8) {
        if (ok)
            *ok = false;
        return QColor();
    }
    bool okR, okG, okB, okA = true;
    int r = t.mid(0, 2).toInt(&okR, 16);
    int g = t.mid(2, 2).toInt(&okG, 16);
    int b = t.mid(4, 2).toInt(&okB, 16);
    int a = 255;
    if (len == 8) {
        a = t.mid(6, 2).toInt(&okA, 16);
    }
    localOk = okR && okG && okB && okA;
    if (ok)
        *ok = localOk;
    if (!localOk)
        return QColor();
    return QColor(r, g, b, a);
}

} // namespace fluent::basicinput
