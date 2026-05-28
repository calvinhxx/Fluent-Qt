#include "ColorPicker.h"

#include "view/textfields/Label.h"
#include "view/textfields/LineEdit.h"
#include "view/basicinput/Slider.h"
#include <QIntValidator>
#include <QRegularExpressionValidator>
#include <QSlider>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>

namespace view::basicinput {

// 内部辅助控件：色谱和色相条 -------------------------------

class ColorSpectrumWidget : public QWidget {
public:
    explicit ColorSpectrumWidget(ColorPicker* picker, QWidget* parent = nullptr)
        : QWidget(parent), m_picker(picker) {
        setMouseTracking(true);
        setCursor(Qt::CrossCursor);
    }

protected:
    void paintEvent(QPaintEvent*) override {
        if (!m_picker) return;
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        QRectF r = rect().adjusted(1, 1, -1, -1);

        // 使用圆角裁剪色盘区域，半径与 Fluent 主题保持一致
        qreal radius = m_picker->themeRadius().control;
        QPainterPath clipPath;
        clipPath.addRoundedRect(r, radius, radius);
        p.setClipPath(clipPath);

        // 基础：从白色到当前色相的水平渐变（S）
        QColor hueColor = QColor::fromHsvF(m_picker->hue(), 1.0, 1.0);
        QLinearGradient gradH(r.topLeft(), r.topRight());
        gradH.setColorAt(0.0, Qt::white);
        gradH.setColorAt(1.0, hueColor);
        p.fillRect(r, gradH);

        // 叠加：从透明到黑色的垂直渐变（V）
        QLinearGradient gradV(r.topLeft(), r.bottomLeft());
        gradV.setColorAt(0.0, QColor(0, 0, 0, 0));
        gradV.setColorAt(1.0, QColor(0, 0, 0, 255));
        p.fillRect(r, gradV);

        // 绘制当前选择位置
        qreal sx = m_picker->saturation();
        qreal sv = 1.0 - m_picker->value();
        QPointF pos(r.left() + sx * r.width(),
                    r.top() + sv * r.height());
        p.setPen(QPen(Qt::white, 2));
        p.setBrush(Qt::NoBrush);
        p.drawEllipse(pos, 6, 6);
    }

    void mousePressEvent(QMouseEvent* e) override {
        setCursor(Qt::BlankCursor);
        handlePos(e->pos());
    }
    void mouseMoveEvent(QMouseEvent* e) override {
        if (e->buttons() & Qt::LeftButton) handlePos(e->pos());
    }
    void mouseReleaseEvent(QMouseEvent* e) override {
        setCursor(Qt::CrossCursor);
    }

private:
    void handlePos(const QPoint& pnt) {
        if (!m_picker) return;
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

class HueBarWidget : public QWidget {
public:
    explicit HueBarWidget(ColorPicker* picker, QWidget* parent = nullptr)
        : QWidget(parent), m_picker(picker) {
        setMouseTracking(true);
        setCursor(Qt::ArrowCursor);
    }

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        QRectF r = rect().adjusted(2, 2, -2, -2);

        QLinearGradient grad(r.topLeft(), r.bottomLeft());
        grad.setColorAt(0.0, QColor::fromHsvF(0.0, 1.0, 1.0));   // red
        grad.setColorAt(1.0/6.0, QColor::fromHsvF(1.0/6.0, 1.0, 1.0)); // yellow
        grad.setColorAt(2.0/6.0, QColor::fromHsvF(2.0/6.0, 1.0, 1.0)); // green
        grad.setColorAt(3.0/6.0, QColor::fromHsvF(3.0/6.0, 1.0, 1.0)); // cyan
        grad.setColorAt(4.0/6.0, QColor::fromHsvF(4.0/6.0, 1.0, 1.0)); // blue
        grad.setColorAt(5.0/6.0, QColor::fromHsvF(5.0/6.0, 1.0, 1.0)); // magenta
        grad.setColorAt(1.0, QColor::fromHsvF(1.0, 1.0, 1.0));   // back to red

        p.fillRect(r, grad);

        if (m_picker) {
            qreal h = m_picker->hue();
            qreal y = r.top() + h * r.height();
            p.setPen(QPen(Qt::white, 2));
            p.drawLine(QPointF(r.left(), y), QPointF(r.right(), y));
        }
    }

    void mousePressEvent(QMouseEvent* e) override {
        setCursor(Qt::BlankCursor);
        handlePos(e->pos());
    }
    void mouseMoveEvent(QMouseEvent* e) override {
        if (e->buttons() & Qt::LeftButton) handlePos(e->pos());
    }
    void mouseReleaseEvent(QMouseEvent* e) override {
        setCursor(Qt::ArrowCursor);
    }

private:
    void handlePos(const QPoint& pnt) {
        if (!m_picker) return;
        QRectF r = rect().adjusted(2, 2, -2, -2);
        qreal h = (pnt.y() - r.top()) / r.height();
        h = std::clamp(h, 0.0, 1.0);
        m_picker->setHueFromBar(h);
        update();
    }

    ColorPicker* m_picker = nullptr;
};

// 右侧预览 pane：棋盘格 + 当前色（含 alpha），参考 WinUI 3
class ColorPreviewPaneWidget : public QWidget {
public:
    explicit ColorPreviewPaneWidget(ColorPicker* picker, QWidget* parent = nullptr)
        : QWidget(parent), m_picker(picker) {
        setFixedWidth(28);
        setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
    }

protected:
    void paintEvent(QPaintEvent*) override {
        if (!m_picker) return;
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

ColorPicker::ColorPicker(QWidget* parent)
    : QWidget(parent) {
    initUi();
    setColor(QColor(255, 255, 255, 255));
}

void ColorPicker::initUi() {
    using Edge = ::view::AnchorLayout::Edge;
    auto* anchorLayout = new ::view::AnchorLayout(this);
    const auto& spacing = themeSpacing();
    const int pad = spacing.padding.card;
    const int gap = spacing.gap.normal;

    // 输入区：Hex + RGBA 五行
    auto* inputsPanel = new QWidget(this);
    auto* inputsLayout = new QVBoxLayout(inputsPanel);
    inputsLayout->setContentsMargins(0, 0, 0, 0);
    inputsLayout->setSpacing(spacing.gap.tight);

    auto createTextRow = [&](const QString& labelText,
                             view::textfields::LineEdit*& edit,
                             void (ColorPicker::*slot)()) {
        auto* row = new QHBoxLayout();
        row->setContentsMargins(0, 0, 0, 0);
        row->setSpacing(spacing.gap.tight);
        auto* label = new view::textfields::Label(labelText, inputsPanel);
        label->setFixedWidth(48);
        row->addWidget(label);
        edit = new view::textfields::LineEdit(inputsPanel);
        edit->setClearButtonEnabled(false);
        row->addWidget(edit, 1);
        connect(edit, &QLineEdit::returnPressed, this, slot);
        inputsLayout->addLayout(row);
    };
    createTextRow("Hex:",   m_hexEdit, &ColorPicker::handleHexEdited);
    createTextRow("Red:",   m_rEdit,   &ColorPicker::handleChannelEdited);
    createTextRow("Green:", m_gEdit,   &ColorPicker::handleChannelEdited);
    createTextRow("Blue:",  m_bEdit,   &ColorPicker::handleChannelEdited);
    m_alphaInputRowWidget = new QWidget(inputsPanel);
    auto* alphaInputRow = new QHBoxLayout(m_alphaInputRowWidget);
    alphaInputRow->setContentsMargins(0, 0, 0, 0);
    alphaInputRow->setSpacing(spacing.gap.tight);
    auto* alphaInputLabel = new view::textfields::Label("Alpha:", m_alphaInputRowWidget);
    alphaInputLabel->setFixedWidth(48);
    m_aEdit = new view::textfields::LineEdit(m_alphaInputRowWidget);
    m_aEdit->setClearButtonEnabled(false);
    alphaInputRow->addWidget(alphaInputLabel);
    alphaInputRow->addWidget(m_aEdit, 1);
    connect(m_aEdit, &QLineEdit::returnPressed, this, &ColorPicker::handleChannelEdited);
    inputsLayout->addWidget(m_alphaInputRowWidget);

    m_hexEdit->setValidator(new QRegularExpressionValidator(
        QRegularExpression("^#?[0-9A-Fa-f]{0,8}$"), m_hexEdit));
    m_rEdit->setValidator(new QIntValidator(0, 255, m_rEdit));
    m_gEdit->setValidator(new QIntValidator(0, 255, m_gEdit));
    m_bEdit->setValidator(new QIntValidator(0, 255, m_bEdit));
    m_aEdit->setValidator(new QIntValidator(0, 255, m_aEdit));

    ::view::AnchorLayout::Anchors aInputs;
    aInputs.left = {this, Edge::Left, pad};
    aInputs.right = {this, Edge::Right, -pad};
    aInputs.bottom = {this, Edge::Bottom, -pad};
    anchorLayout->addAnchoredWidget(inputsPanel, aInputs);

    // 下方 Slider 区：明度 (V) + Alpha，参考 WinUI 3
    auto* slidersPanel = new QWidget(this);
    auto* slidersLayout = new QVBoxLayout(slidersPanel);
    slidersLayout->setContentsMargins(0, 0, 0, 0);
    slidersLayout->setSpacing(spacing.gap.tight);

    auto* valueRow = new QHBoxLayout();
    valueRow->setContentsMargins(0, 0, 0, 0);
    valueRow->setSpacing(spacing.gap.tight);
    auto* valueLabel = new view::textfields::Label("Value:", slidersPanel);
    valueLabel->setFixedWidth(48);
    m_valueSlider = new Slider(Qt::Horizontal, slidersPanel);
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
    auto* alphaLabel = new view::textfields::Label("Alpha:", m_alphaRowWidget);
    alphaLabel->setFixedWidth(48);
    m_alphaSlider = new Slider(Qt::Horizontal, m_alphaRowWidget);
    m_alphaSlider->setMinimum(0);
    m_alphaSlider->setMaximum(255);
    m_alphaSlider->setValue(255);
    connect(m_alphaSlider, &QSlider::valueChanged, this, &ColorPicker::setAlphaFromSlider);
    alphaRow->addWidget(alphaLabel);
    alphaRow->addWidget(m_alphaSlider, 1);
    slidersLayout->addWidget(m_alphaRowWidget);

    ::view::AnchorLayout::Anchors aSliders;
    aSliders.left = {this, Edge::Left, pad};
    aSliders.right = {this, Edge::Right, -pad};
    aSliders.bottom = {inputsPanel, Edge::Top, -gap};
    anchorLayout->addAnchoredWidget(slidersPanel, aSliders);

    m_spectrum = new ColorSpectrumWidget(this, this);
    m_spectrum->setMinimumSize(200, 160);
    m_spectrum->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    m_hueBar = new HueBarWidget(this, this);
    m_hueBar->setFixedWidth(24);

    m_previewPane = new ColorPreviewPaneWidget(this, this);

    ::view::AnchorLayout::Anchors aSpectrum;
    aSpectrum.left = {this, Edge::Left, pad};
    aSpectrum.top = {this, Edge::Top, pad};
    aSpectrum.right = {m_hueBar, Edge::Left, -gap};
    aSpectrum.bottom = {slidersPanel, Edge::Top, -gap};
    anchorLayout->addAnchoredWidget(m_spectrum, aSpectrum);

    ::view::AnchorLayout::Anchors aHue;
    aHue.right = {m_previewPane, Edge::Left, -gap};
    aHue.left = {m_hueBar, Edge::Right, -24};
    aHue.top = {m_spectrum, Edge::Top, 0};
    aHue.bottom = {m_spectrum, Edge::Bottom, 0};
    anchorLayout->addAnchoredWidget(m_hueBar, aHue);

    ::view::AnchorLayout::Anchors aPreview;
    aPreview.right = {this, Edge::Right, -pad};
    aPreview.left = {m_previewPane, Edge::Right, -28};
    aPreview.top = {m_spectrum, Edge::Top, 0};
    aPreview.bottom = {m_spectrum, Edge::Bottom, 0};
    anchorLayout->addAnchoredWidget(m_previewPane, aPreview);

    onThemeUpdated();
}

void ColorPicker::onThemeUpdated() {
    const auto& colors = themeColors();
    const auto& fs = themeFont("Body");
    QFont f = fs.toQFont();

    // 1. 更新可能无法自动处理父级字体更改的内部主题感知子控件
    const auto labels = findChildren<view::textfields::Label*>();
    for (auto* label : labels) {
        label->onThemeUpdated();
    }

    setFont(f);

    // 2. 同步输入框主题样式
    if (m_hexEdit) m_hexEdit->onThemeUpdated();
    if (m_rEdit)   m_rEdit->onThemeUpdated();
    if (m_gEdit)   m_gEdit->onThemeUpdated();
    if (m_bEdit)   m_bEdit->onThemeUpdated();
    if (m_aEdit)   m_aEdit->onThemeUpdated();

    updateFromColor();
}

void ColorPicker::setColor(const QColor& c) {
    if (c == m_color)
        return;
    m_color = c;

    // 同步 HSV 分量
    FluentColorComponent h, s, v, a;
    c.getHsvF(&h, &s, &v, &a);

    // 1. 如果新颜色是灰度 (h < 0)，保持现有色相，避免色相丢失
    if (h < 0.0f) {
        h = m_h;
    }

    // 2. 如果新颜色是黑色 (v ~ 0)，保持现有饱和度，避免面板拖拽到底部时 Handle 跳到左侧 (s=0)
    if (qFuzzyIsNull(v)) {
        s = m_s;
    }

    m_h = h;
    m_s = s;
    m_v = v;

    updateFromColor();
    emit colorChanged(m_color);
}

void ColorPicker::setAlphaEnabled(bool enabled) {
    if (m_alphaEnabled == enabled)
        return;
    m_alphaEnabled = enabled;
    if (m_alphaRowWidget) m_alphaRowWidget->setVisible(enabled);
    if (m_alphaInputRowWidget) m_alphaInputRowWidget->setVisible(enabled);
    updateFromColor();
    emit alphaEnabledChanged(enabled);
}

void ColorPicker::setHueFromBar(qreal h) {
    m_h = h;
    // 基于当前 HSV 重建颜色
    QColor c = QColor::fromHsvF(m_h, m_s, m_v, m_color.alphaF());
    setColor(c);
    if (m_spectrum) m_spectrum->update();
}

void ColorPicker::setSVFromSpectrum(qreal s, qreal v) {
    m_s = s;
    m_v = v;
    QColor c = QColor::fromHsvF(m_h, m_s, m_v, m_color.alphaF());
    setColor(c);
}

void ColorPicker::setValueFromSlider(int percent) {
    if (m_isInternalUpdate) return;
    m_v = std::clamp(percent / 100.0, 0.0, 1.0);
    QColor c = QColor::fromHsvF(m_h, m_s, m_v, m_color.alphaF());
    setColor(c);
}

void ColorPicker::setAlphaFromSlider(int alpha) {
    if (m_isInternalUpdate) return;
    alpha = std::clamp(alpha, 0, 255);
    QColor c = m_color;
    c.setAlpha(alpha);
    setColor(c);
}

void ColorPicker::handleChannelEdited() {
    if (m_isInternalUpdate) return;

    if (!m_rEdit || !m_gEdit || !m_bEdit || !m_aEdit)
        return;

    int r = m_rEdit->text().toInt();
    int g = m_gEdit->text().toInt();
    int b = m_bEdit->text().toInt();
    int a = m_alphaEnabled ? m_aEdit->text().toInt() : 255;

    // 验证器通常会限制 0-255 的范围，但 QIntValidator 允许中间输入，因此为了安全再次约束
    r = std::clamp(r, 0, 255);
    g = std::clamp(g, 0, 255);
    b = std::clamp(b, 0, 255);
    a = std::clamp(a, 0, 255);

    QColor c(r, g, b, a);
    setColor(c);
}

void ColorPicker::handleHexEdited() {
    if (m_isInternalUpdate) return; // 内部更新时忽略，保持状态一致

    bool ok = false;
    QColor c = hexToColor(m_hexEdit->text().trimmed(), &ok);
    if (ok) {
        setColor(c);
    } else {
        // 回退到当前颜色的 Hex
        updateFromColor();
    }
}

void ColorPicker::updateFromColor() {
    m_isInternalUpdate = true;
    if (m_rEdit && m_gEdit && m_bEdit && m_aEdit) {
        m_rEdit->setText(QString::number(m_color.red()));
        m_gEdit->setText(QString::number(m_color.green()));
        m_bEdit->setText(QString::number(m_color.blue()));
        m_aEdit->setText(QString::number(m_color.alpha()));
    }
    if (m_hexEdit) m_hexEdit->setText(colorToHex(m_color, m_alphaEnabled));
    if (m_valueSlider) m_valueSlider->setValue(qRound(m_v * 100));
    if (m_alphaSlider) m_alphaSlider->setValue(m_color.alpha());
    if (m_spectrum) m_spectrum->update();
    if (m_hueBar) m_hueBar->update();
    if (m_previewPane) m_previewPane->update();
    m_isInternalUpdate = false;
}

QString ColorPicker::colorToHex(const QColor& c, bool withAlpha) const {
    QString hex = QString("#%1%2%3")
                      .arg(c.red(), 2, 16, QLatin1Char('0'))
                      .arg(c.green(), 2, 16, QLatin1Char('0'))
                      .arg(c.blue(), 2, 16, QLatin1Char('0'))
                      .toUpper();
    if (withAlpha) {
        hex += QString("%1").arg(c.alpha(), 2, 16, QLatin1Char('0')).toUpper();
    }
    return hex;
}

QColor ColorPicker::hexToColor(const QString& text, bool* ok) const {
    QString t = text;
    if (t.startsWith('#'))
        t.remove(0, 1);
    bool localOk = false;
    int len = t.length();
    if (len != 6 && len != 8) {
        if (ok) *ok = false;
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
    if (ok) *ok = localOk;
    if (!localOk) return QColor();
    return QColor(r, g, b, a);
}

} // namespace view::basicinput

