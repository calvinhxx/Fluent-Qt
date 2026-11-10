#include "QtTestEnvironment.h"
#include "components/basicinput/Button.h"
#include "components/basicinput/ComboBox.h"
#include "components/basicinput/DropDownButton.h"
#include "components/basicinput/MultiSelectComboBox.h"
#include "components/textfields/AutoSuggestBox.h"
#include "components/textfields/NumberBox.h"
#include "components/textfields/PasswordBox.h"
#include "components/textfields/TagInput.h"
#include "components/foundation/FluentElement.h"
#include "components/foundation/QMLPlus.h"
#include "components/foundation/ThemeRegistry.h"
#include "components/menus_toolbars/Menu.h"
#include "components/textfields/Label.h"
#include "components/textfields/LineEdit.h"
#include "design/Spacing.h"
#include "design/Typography.h"
#include <QAction>
#include <QAccessible>
#include <QApplication>
#include <QContextMenuEvent>
#include <QImage>
#include <QIntValidator>
#include <QHBoxLayout>
#include <QMenu>
#include <QPainter>
#include <QTimer>
#include <QToolButton>
#include <QtMath>
#include <QtTest/QSignalSpy>
#include <QtTest/QTest>
#include <gtest/gtest.h>

using namespace fluent::textfields;
using namespace fluent::basicinput;
using namespace fluent;

TEST(FieldTrailingContentTest, TextIsDecorationAndPreservesCallerMargins)
{
    for (auto direction : {Qt::LeftToRight, Qt::RightToLeft}) {
        LineEdit edit;
        edit.resize(240, 32);
        edit.setLayoutDirection(direction);
        edit.setText("1280");
        const QMargins margins(3, 1, 5, 2);
        edit.setTextMargins(margins);
        QSignalSpy values(&edit, &QLineEdit::textChanged);
        QSignalSpy suffix(&edit, &LineEdit::trailingTextChanged);
        edit.setTrailingText("px");
        edit.setTrailingText("px");
        edit.show();
        QApplication::processEvents();
        auto* label = edit.findChild<Label*>("fluentFieldTrailingText");
        ASSERT_NE(label, nullptr);
        EXPECT_EQ(label->text(), QString("px"));
        EXPECT_FALSE(label->isTextElided());
        EXPECT_EQ(label->font().pixelSize(), Typography::FontSize::Caption);
        EXPECT_EQ(edit.text(), QString("1280"));
        EXPECT_EQ(edit.textMargins(), margins);
        EXPECT_EQ(values.count(), 0);
        EXPECT_EQ(suffix.count(), 1);
        EXPECT_TRUE(edit.rect().contains(label->geometry()));
        EXPECT_EQ(label->focusPolicy(), Qt::NoFocus);
        EXPECT_EQ(label->geometry().center().x() > edit.width() / 2, direction == Qt::LeftToRight);
        edit.setEnabled(false);
        EXPECT_EQ(label->textColorRole(), Label::TextColorRole::Disabled);
        edit.setEnabled(true);
        EXPECT_EQ(label->textColorRole(), Label::TextColorRole::Secondary);
        edit.selectAll();
        EXPECT_EQ(edit.selectedText(), QString("1280"));
        edit.setTrailingText(QString());
        EXPECT_TRUE(label->isHidden());
        EXPECT_EQ(edit.textMargins(), margins);
    }
}

TEST(FieldTrailingContentTest, OwnershipReplacementTakeAndHostDestruction)
{
    for (auto ownership : {WidgetOwnership::Borrowed, WidgetOwnership::Reparented, WidgetOwnership::Owned}) {
        QWidget original;
        auto* edit = new LineEdit;
        QPointer<Button> child = new Button("Action", &original);
        ASSERT_TRUE(edit->setTrailingWidget(child, ownership));
        EXPECT_EQ(child->parentWidget(), edit);
        EXPECT_FALSE(edit->setTrailingWidget(edit, ownership));
        EXPECT_FALSE(edit->setTrailingWidget(child,
            ownership == WidgetOwnership::Owned ? WidgetOwnership::Borrowed : WidgetOwnership::Owned));
        delete edit;
        if (ownership == WidgetOwnership::Owned) {
            EXPECT_TRUE(child.isNull());
        } else {
            ASSERT_FALSE(child.isNull());
            EXPECT_EQ(child->parentWidget(), ownership == WidgetOwnership::Reparented ? &original : nullptr);
            delete child.data();
        }
    }
    LineEdit edit;
    QPointer<Button> owned = new Button("Action");
    ASSERT_TRUE(edit.setTrailingWidget(owned, WidgetOwnership::Owned));
    EXPECT_EQ(edit.takeTrailingWidget(), owned.data());
    EXPECT_EQ(owned->parentWidget(), nullptr);
    ASSERT_TRUE(edit.setTrailingWidget(owned, WidgetOwnership::Owned));
    edit.setTrailingText("px");
    EXPECT_TRUE(owned.isNull());
    EXPECT_EQ(edit.trailingWidget(), nullptr);
    EXPECT_EQ(edit.trailingText(), QString("px"));
}

TEST(FieldTrailingContentTest, WidgetSizeVisibilityAndExternalDeletionUpdateReservation)
{
    LineEdit edit;
    edit.resize(320, 40);
    auto* button = new Button("Reset");
    button->setFixedSize(48, 24);
    ASSERT_TRUE(edit.setTrailingWidget(button, WidgetOwnership::Borrowed));
    edit.show();
    QApplication::processEvents();
    EXPECT_EQ(button->width(), 48);
    const QString firstStyle = edit.styleSheet();
    button->setFixedWidth(80);
    QApplication::processEvents();
    EXPECT_NE(edit.styleSheet(), firstStyle);
    const QString wideStyle = edit.styleSheet();
    button->hide();
    QApplication::processEvents();
    EXPECT_NE(edit.styleSheet(), wideStyle);
    button->show();
    QApplication::processEvents();
    EXPECT_EQ(edit.styleSheet(), wideStyle);
    QSignalSpy changed(&edit, &LineEdit::trailingWidgetChanged);
    delete button;
    EXPECT_EQ(edit.trailingWidget(), nullptr);
    EXPECT_EQ(changed.count(), 1);
}

TEST(FieldTrailingContentTest, NativeActionsAndClearRemainSeparateFromSuffix)
{
    for (auto direction : {Qt::LeftToRight, Qt::RightToLeft}) {
        LineEdit edit;
        edit.setLayoutDirection(direction);
        edit.resize(320, 32);
        edit.setText("1280");
        edit.setClearButtonFocusPolicy(Qt::StrongFocus);
        edit.setClearButtonAccessibleName("Clear width");
        edit.setTrailingText("px");
        QPixmap pixmap(16, 16);
        pixmap.fill(Qt::blue);
        auto* action = edit.addAction(QIcon(pixmap), QLineEdit::TrailingPosition);
        edit.show();
        QApplication::processEvents();
        auto* suffix = edit.findChild<Label*>("fluentFieldTrailingText");
        auto* clear = edit.findChild<Button*>("fluentLineEditClearButton");
        ASSERT_NE(suffix, nullptr);
        ASSERT_NE(clear, nullptr);
        EXPECT_EQ(clear->accessibleName(), QString("Clear width"));
        EXPECT_EQ(clear->focusPolicy(), Qt::StrongFocus);
        EXPECT_FALSE(suffix->geometry().intersects(clear->geometry()));
        bool found = false;
        for (auto* native : edit.findChildren<QToolButton*>()) {
            if (native->defaultAction() == action) {
                found = true;
                EXPECT_FALSE(native->geometry().intersects(clear->geometry()));
                EXPECT_FALSE(native->geometry().intersects(suffix->geometry()));
            }
        }
        EXPECT_TRUE(found);
        edit.removeAction(action);
        delete action;
        QApplication::processEvents();
        EXPECT_FALSE(suffix->geometry().intersects(clear->geometry()));
    }
}

TEST(FieldTrailingContentTest, ExternalReparentAndReentrantNotificationPreserveLifetime)
{
    LineEdit edit;
    QWidget destination;
    Button child("Action");
    ASSERT_TRUE(edit.setTrailingWidget(&child, WidgetOwnership::Borrowed));
    child.setParent(&destination);
    EXPECT_EQ(edit.trailingWidget(), nullptr);
    EXPECT_EQ(child.parentWidget(), &destination);
    child.setParent(nullptr);
    auto* container = new QWidget;
    auto* nested = new Button("Nested", container);
    ASSERT_TRUE(edit.setTrailingWidget(container, WidgetOwnership::Owned));
    EXPECT_FALSE(edit.setTrailingWidget(nested, WidgetOwnership::Owned));
    EXPECT_EQ(edit.trailingWidget(), container);
    QPointer<LineEdit> host = new LineEdit;
    auto* action = new Button("Action");
    QObject::connect(host, &LineEdit::trailingWidgetChanged, host, [host] { delete host.data(); });
    host->setTrailingWidget(action, WidgetOwnership::Borrowed);
    EXPECT_TRUE(host.isNull());
    EXPECT_EQ(action->parentWidget(), nullptr);
    delete action;
}

TEST(FieldTrailingContentTest, KeyboardCanLeaveCustomActionInDerivedFields)
{
    for (int kind = 0; kind < 3; ++kind) {
        QWidget host;
        QHBoxLayout layout(&host);
        LineEdit* edit = kind == 0 ? new LineEdit : kind == 1 ? new AutoSuggestBox : new TagInput;
        edit->setClearButtonEnabled(false);
        auto* action = new Button("Reset");
        action->setFixedSize(48, 24);
        action->setFocusPolicy(Qt::StrongFocus);
        edit->setTrailingWidget(action, WidgetOwnership::Owned);
        Button after("Next");
        layout.addWidget(edit);
        layout.addWidget(&after);
        host.show();
        QApplication::setActiveWindow(&host);
        action->setFocus(Qt::TabFocusReason);
        ASSERT_TRUE(QTest::qWaitFor([&] { return action->hasFocus(); }, 1000));
        QTest::keyClick(action, Qt::Key_Tab);
        EXPECT_TRUE(after.hasFocus());
        QTest::keyClick(&after, Qt::Key_Backtab);
        EXPECT_TRUE(action->hasFocus());
    }
}

TEST(FieldTrailingContentTest, DerivedFieldsReserveChromeAndHeaderSpace)
{
    for (auto direction : {Qt::LeftToRight, Qt::RightToLeft}) {
        NumberBox number;
        PasswordBox password;
        TagInput tags;
        number.setHeader("Width");
        password.setHeader("Password");
        password.setText("example");
        tags.setHeader("Members");
        tags.addItem("ada", "Ada");
        for (LineEdit* edit :
            {static_cast<LineEdit*>(&number), static_cast<LineEdit*>(&password), static_cast<LineEdit*>(&tags)}) {
            edit->setLayoutDirection(direction);
            edit->resize(280, edit->sizeHint().height());
            edit->setTrailingText("unit");
            edit->show();
            QApplication::processEvents();
            auto* suffix = edit->findChild<Label*>("fluentFieldTrailingText");
            ASSERT_NE(suffix, nullptr);
            EXPECT_TRUE(edit->rect().contains(suffix->geometry()));
            EXPECT_GE(suffix->y(), 24);
            for (auto* child : edit->findChildren<QWidget*>(QString(), Qt::FindDirectChildrenOnly)) {
                if (child != suffix && child->isVisible()
                    && (qobject_cast<Button*>(child) || child->objectName() == "TagInputTag"))
                    EXPECT_FALSE(suffix->geometry().intersects(child->geometry())) << child->objectName().toStdString();
            }
        }
    }
}

TEST(InputMetricsTest, StandardSingleLineControlsAlignWithoutPerSampleSizing)
{
    QWidget row;
    auto* layout = new QHBoxLayout(&row);
    QList<QWidget*> controls{new LineEdit(&row), new Button("Apply", &row), new DropDownButton("More", &row),
        new ComboBox(&row), new MultiSelectComboBox(&row), new NumberBox(&row)};
    for (auto* control : controls)
        layout->addWidget(control);
    row.resize(1000, 100);
    row.show();
    QApplication::processEvents();
    for (auto* control : controls) {
        SCOPED_TRACE(control->metaObject()->className());
        EXPECT_EQ(control->height(), Spacing::ControlHeight::Standard);
        EXPECT_EQ(control->y(), controls.first()->y());
    }
    for (int pixelSize : {18, 28, 36}) {
        SCOPED_TRACE(pixelSize);
        QFont large = controls.first()->font();
        large.setPixelSize(pixelSize);
        for (auto* control : controls)
            control->setFont(large);
        QApplication::processEvents();
        for (auto* control : controls) {
            SCOPED_TRACE(control->metaObject()->className());
            EXPECT_EQ(control->height(), controls.first()->height());
            EXPECT_GE(control->height(), control->fontMetrics().height() + 8);
            EXPECT_EQ(control->y(), controls.first()->y());
        }
    }
}

namespace {

class FluentTestWindow : public QWidget, public fluent::FluentElement {
public:
    using QWidget::QWidget;
    void onThemeUpdated() override
    {
        const auto& c = themeColors();
        setStyleSheet(QString("background-color: %1;").arg(c.bgCanvas.name()));
    }
};

} // namespace

class LineEditTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        window = new FluentTestWindow();
        window->setFixedSize(500, 400);
        window->setWindowTitle("Fluent LineEdit Test");
        layout = new AnchorLayout(window);
        window->setLayout(layout);
        window->onThemeUpdated();
    }

    void TearDown() override { delete window; }

    FluentTestWindow* window;
    AnchorLayout* layout;
};

namespace {

bool actionMatchesStandardKey(const QAction* action, QKeySequence::StandardKey standardKey)
{
    if (!action)
        return false;

    QList<QKeySequence> shortcuts = action->shortcuts();
    if (shortcuts.isEmpty()) {
        const int tabIndex = action->text().indexOf(QLatin1Char('\t'));
        if (tabIndex >= 0) {
            const QKeySequence embedded(action->text().mid(tabIndex + 1).trimmed(), QKeySequence::NativeText);
            if (!embedded.isEmpty())
                shortcuts.append(embedded);
        }
    }

    const QList<QKeySequence> bindings = QKeySequence::keyBindings(standardKey);
    for (const QKeySequence& shortcut : shortcuts) {
        for (const QKeySequence& binding : bindings) {
            if (shortcut.matches(binding) == QKeySequence::ExactMatch)
                return true;
        }
    }
    return false;
}

bool triggerContextAction(LineEdit* edit, QKeySequence::StandardKey standardKey)
{
    if (!edit)
        return false;

    bool triggered = false;
    QTimer::singleShot(0, [&]() {
        auto* menu = qobject_cast<fluent::menus_toolbars::FluentMenu*>(QApplication::activePopupWidget());
        if (!menu)
            return;

        for (QAction* action : menu->actions()) {
            if (!actionMatchesStandardKey(action, standardKey))
                continue;
            if (action->isEnabled()) {
                action->trigger();
                triggered = true;
            }
            break;
        }
        if (menu->isVisible())
            menu->close();
    });

    const QPoint localPos = edit->rect().center();
    const QPoint globalPos = edit->mapToGlobal(localPos);
    QContextMenuEvent event(QContextMenuEvent::Mouse, localPos, globalPos);
    QApplication::sendEvent(edit, &event);
    QTest::qWait(1);
    return triggered;
}

} // namespace

TEST_F(LineEditTest, TextAndPlaceholder)
{
    LineEdit* edit = new LineEdit(window);
    edit->setPlaceholderText("Enter value");
    EXPECT_EQ(edit->placeholderText(), "Enter value");

    edit->setText("hello");
    EXPECT_EQ(edit->text(), "hello");
}

TEST_F(LineEditTest, PlaceholderPaletteUsesResolvedOpaqueToken)
{
    const auto previousTheme = fluent::FluentElement::currentTheme();
    fluent::FluentElement::setTheme(fluent::FluentElement::Light);

    LineEdit edit(window);
    edit.onThemeUpdated();
    const QColor placeholder = edit.palette().color(QPalette::Active, QPalette::PlaceholderText);

    EXPECT_EQ(placeholder.alpha(), 255);
    EXPECT_GT(placeholder.red(), 130);
    EXPECT_LT(placeholder.red(), 150);
    EXPECT_EQ(placeholder.red(), placeholder.green());
    EXPECT_EQ(placeholder.green(), placeholder.blue());

    fluent::FluentElement::setTheme(previousTheme);
}

TEST_F(LineEditTest, StyledAncestorDoesNotReplaceDarkThemeTextPalette)
{
    const auto previousTheme = fluent::FluentElement::currentTheme();
    fluent::FluentElement::setTheme(fluent::FluentElement::Light);
    window->onThemeUpdated();

    LineEdit edit(window);
    edit.setText(QStringLiteral("42"));
    window->show();
    QApplication::processEvents();
    fluent::FluentElement::setTheme(fluent::FluentElement::Dark);
    edit.onThemeUpdated();

    const auto colors = edit.themeColors();
    EXPECT_EQ(edit.palette().color(QPalette::Active, QPalette::Text), colors.textPrimary);
    EXPECT_EQ(edit.palette().color(QPalette::Inactive, QPalette::Text), colors.textPrimary);
    EXPECT_EQ(edit.palette().color(QPalette::Disabled, QPalette::Text), colors.textDisabled);

    // Ancestors can be notified after children because the theme registry is unordered.
    window->setStyleSheet(QStringLiteral("background-color: #202020; border: none;"));
    EXPECT_EQ(edit.palette().color(QPalette::Active, QPalette::Text), colors.textPrimary);
    EXPECT_EQ(edit.palette().color(QPalette::Inactive, QPalette::Text), colors.textPrimary);
    EXPECT_EQ(edit.palette().color(QPalette::Disabled, QPalette::Text), colors.textDisabled);

    fluent::FluentElement::setTheme(previousTheme);
    window->onThemeUpdated();
}

TEST_F(LineEditTest, ContentMargins)
{
    LineEdit* edit = new LineEdit(window);
    QMargins margins(10, 2, 10, 2);
    edit->setContentMargins(margins);
    EXPECT_EQ(edit->contentMargins(), margins);
}

TEST_F(LineEditTest, ReadOnly)
{
    LineEdit* edit = new LineEdit(window);
    edit->setText("read only");
    edit->setReadOnly(true);
    EXPECT_TRUE(edit->isReadOnly());
    edit->setReadOnly(false);
    EXPECT_FALSE(edit->isReadOnly());
}

TEST_F(LineEditTest, Validator)
{
    LineEdit* edit = new LineEdit(window);
    auto* validator = new QIntValidator(0, 100, edit);
    edit->setValidator(validator);
    EXPECT_EQ(edit->validator(), validator);
}

TEST_F(LineEditTest, Contract_StandardEditingActionsUseFluentContextMenu)
{
    auto* edit = new LineEdit(window);
    edit->setText(QStringLiteral("Alpha Beta"));
    edit->selectAll();
    layout->addWidget(edit);
    window->show();
    QApplication::processEvents();

    bool sawFluentMenu = false;
    bool sawCopy = false;
    bool sawSelectAll = false;
    bool sawCopyGlyph = false;
    bool sawDeleteGlyph = false;
    bool sawSelectAllGlyph = false;
    QTimer::singleShot(0, [&]() {
        auto* menu = qobject_cast<fluent::menus_toolbars::FluentMenu*>(QApplication::activePopupWidget());
        sawFluentMenu = menu != nullptr;
        if (!menu)
            return;

        EXPECT_EQ(menu->objectName(), QStringLiteral("FluentLineEdit.ContextMenu"));
        EXPECT_EQ(menu->fontStyle(), Typography::FontRole::Caption);
        EXPECT_EQ(menu->font().pixelSize(), Typography::FontSize::Caption);
        EXPECT_FALSE(menu->property("_fluentqt_menuQuietSeparators").toBool());
        for (QAction* action : menu->actions()) {
            if (!action->isSeparator()) {
                EXPECT_LT(menu->actionGeometry(action).height(), ::Spacing::ControlHeight::Standard);
            }
            const bool isCopy = actionMatchesStandardKey(action, QKeySequence::Copy);
            const bool isSelectAll = actionMatchesStandardKey(action, QKeySequence::SelectAll);
            const bool isDelete = action->text().contains(QStringLiteral("Delete"), Qt::CaseInsensitive);
            sawCopy = sawCopy || isCopy;
            sawSelectAll = sawSelectAll || isSelectAll;
            sawCopyGlyph = sawCopyGlyph || (isCopy && !action->icon().isNull());
            sawDeleteGlyph = sawDeleteGlyph || (isDelete && !action->icon().isNull());
            sawSelectAllGlyph = sawSelectAllGlyph || (isSelectAll && !action->icon().isNull());
            if (!action->icon().isNull()) {
                const QSize iconSize = action->icon().actualSize(QSize(64, 64));
                const int maximumBackingExtent = qCeil(
                    Typography::IconSize::Standard * qMax<qreal>(1.0, menu->devicePixelRatioF()));
                EXPECT_GT(iconSize.width(), 0);
                EXPECT_LE(iconSize.width(), maximumBackingExtent);
                EXPECT_LE(iconSize.height(), maximumBackingExtent);
            }
        }
        menu->close();
    });

    const QPoint localPos = edit->rect().center();
    const QPoint globalPos = edit->mapToGlobal(localPos);
    QContextMenuEvent event(QContextMenuEvent::Mouse, localPos, globalPos);
    QApplication::sendEvent(edit, &event);

    EXPECT_TRUE(event.isAccepted());
    ASSERT_TRUE(QTest::qWaitFor([&] { return sawFluentMenu; }, 1000));
    EXPECT_TRUE(sawFluentMenu);
    EXPECT_TRUE(sawCopy);
    EXPECT_TRUE(sawSelectAll);
    EXPECT_TRUE(sawCopyGlyph);
    EXPECT_TRUE(sawDeleteGlyph);
    EXPECT_TRUE(sawSelectAllGlyph);
}

TEST_F(LineEditTest, Contract_UndoRedoRemainFunctionalFromContextMenu)
{
    auto* edit = new LineEdit(window);
    edit->setText(QStringLiteral("Alpha"));
    layout->addWidget(edit);
    window->show();
    QApplication::processEvents();

    edit->setCursorPosition(edit->text().size());
    edit->insert(QStringLiteral(" Beta"));
    const QString editedText = QStringLiteral("Alpha Beta");
    ASSERT_EQ(edit->text(), editedText);

    edit->setFocus(Qt::OtherFocusReason);
    QTest::keySequence(edit, QKeySequence(QKeySequence::Undo));
    EXPECT_EQ(edit->text(), QStringLiteral("Alpha"));
    QTest::keySequence(edit, QKeySequence(QKeySequence::Redo));
    EXPECT_EQ(edit->text(), editedText);

    ASSERT_TRUE(triggerContextAction(edit, QKeySequence::Undo));
    EXPECT_EQ(edit->text(), QStringLiteral("Alpha"));
    ASSERT_TRUE(triggerContextAction(edit, QKeySequence::Redo));
    EXPECT_EQ(edit->text(), editedText);
}

TEST_F(LineEditTest, FluentPropertiesDefaultsAndSetters)
{
    LineEdit* edit = new LineEdit(window);

    // 默认值验证（引用 Spacing/Typography 常量）
    EXPECT_TRUE(edit->isClearButtonEnabled());
    EXPECT_EQ(edit->clearButtonSize(), 24);
    EXPECT_EQ(edit->clearButtonOffset(), QPoint(Spacing::XSmall, 0));
    EXPECT_EQ(edit->focusedBorderWidth(), Spacing::Border::Focused);
    EXPECT_EQ(edit->unfocusedBorderWidth(), 0);
    EXPECT_EQ(edit->fontRole(), Typography::FontRole::Body);

    QSignalSpy spyOffset(edit, SIGNAL(clearButtonOffsetChanged()));
    QSignalSpy spyFocused(edit, SIGNAL(focusedBorderWidthChanged()));
    QSignalSpy spyUnfocused(edit, SIGNAL(unfocusedBorderWidthChanged()));

    edit->setClearButtonOffset(QPoint(10, 3));
    EXPECT_EQ(edit->clearButtonOffset(), QPoint(10, 3));
    EXPECT_EQ(spyOffset.count(), 1);

    edit->setFocusedBorderWidth(3);
    EXPECT_EQ(edit->focusedBorderWidth(), 3);
    EXPECT_EQ(spyFocused.count(), 1);

    edit->setUnfocusedBorderWidth(2);
    EXPECT_EQ(edit->unfocusedBorderWidth(), 2);
    EXPECT_EQ(spyUnfocused.count(), 1);

    // 相同值不应再次触发信号
    edit->setClearButtonOffset(QPoint(10, 3));
    edit->setFocusedBorderWidth(3);
    edit->setUnfocusedBorderWidth(2);
    EXPECT_EQ(spyOffset.count(), 1);
    EXPECT_EQ(spyFocused.count(), 1);
    EXPECT_EQ(spyUnfocused.count(), 1);
}

TEST_F(LineEditTest, Contract_ZeroBorderWidthOmitsUnderlineWithoutPaintingTopEdge)
{
    LineEdit edit(window);
    edit.setGeometry(20, 20, 240, 40);
    edit.setClearButtonEnabled(false);
    const int defaultFocusedWidth = edit.focusedBorderWidth();
    const int defaultUnfocusedWidth = edit.unfocusedBorderWidth();
    window->show();
    window->activateWindow();
    QApplication::processEvents();

    for (auto theme : {FluentElement::Light, FluentElement::Dark}) {
        SCOPED_TRACE(static_cast<int>(theme));
        edit.setProperty("fluentThemeOverride", static_cast<int>(theme));
        for (bool focused : {false, true}) {
            SCOPED_TRACE(focused);
            edit.setFocus(Qt::OtherFocusReason);
            if (!focused)
                edit.clearFocus();
            QApplication::processEvents();
            ASSERT_EQ(edit.hasFocus(), focused);

            for (qreal dpr : {1.0, 1.5, 2.0}) {
                SCOPED_TRACE(dpr);
                const auto render = [&] {
                    QImage image(qRound(edit.width() * dpr), qRound(edit.height() * dpr),
                        QImage::Format_ARGB32_Premultiplied);
                    image.setDevicePixelRatio(dpr);
                    image.fill(Qt::transparent);
                    QPainter painter(&image);
                    edit.render(&painter);
                    return image;
                };
                edit.setFocusedBorderWidth(defaultFocusedWidth);
                edit.setUnfocusedBorderWidth(defaultUnfocusedWidth);
                const QImage defaultFrame = render();
                const int middle = defaultFrame.width() / 2;
                if (focused)
                    EXPECT_NE(defaultFrame.pixelColor(middle, 0),
                        defaultFrame.pixelColor(middle, defaultFrame.height() - 1));
                else
                    EXPECT_EQ(defaultFrame.pixelColor(middle, 0),
                        defaultFrame.pixelColor(middle, defaultFrame.height() - 1));

                edit.setFocusedBorderWidth(2);
                edit.setUnfocusedBorderWidth(2);
                const QImage visible = render();
                if (focused)
                    edit.setFocusedBorderWidth(0);
                else
                    edit.setUnfocusedBorderWidth(0);
                const QImage hidden = render();
                const int x = hidden.width() / 2;
                const int bottom = hidden.height() - 1;
                EXPECT_EQ(hidden.pixelColor(x, 0), visible.pixelColor(x, 0));
                EXPECT_EQ(hidden.pixelColor(x, bottom), visible.pixelColor(x, 0));
                EXPECT_NE(hidden.pixelColor(x, bottom), visible.pixelColor(x, bottom));
                EXPECT_EQ(hidden.pixelColor(x, hidden.height() / 2), visible.pixelColor(x, visible.height() / 2));
            }
        }
    }
}

TEST_F(LineEditTest, Contract_DerivedInputsHonorInheritedFrameConfiguration)
{
    LineEdit reference(window);
    const QVector<LineEdit*> inputs{new AutoSuggestBox(window), new NumberBox(window), new PasswordBox(window),
        new TagInput(window)};
    for (auto* input : inputs)
        input->hide();
    reference.hide();
    window->show();
    window->activateWindow();
    QTest::mouseMove(window, QPoint(1, 1));
    QApplication::processEvents();

    for (auto* input : inputs) {
        SCOPED_TRACE(input->metaObject()->className());
        EXPECT_TRUE(input->isFrameVisible());
        EXPECT_EQ(input->unfocusedBorderWidth(), 0);
        EXPECT_EQ(input->focusedBorderWidth(), Spacing::Border::Focused);
        const int inputHeight = input->sizeHint().height();
        for (bool header : {false, true}) {
            SCOPED_TRACE(header);
            const QString text = header ? QStringLiteral("Input header") : QString();
            if (auto* suggest = qobject_cast<AutoSuggestBox*>(input))
                suggest->setHeader(text);
            else if (auto* number = qobject_cast<NumberBox*>(input))
                number->setHeader(text);
            else if (auto* password = qobject_cast<PasswordBox*>(input))
                password->setHeader(text);
            input->setGeometry(20, 20, 240, input->sizeHint().height());
            reference.setGeometry(20, 20, 240, inputHeight);

            for (auto theme : {FluentElement::Light, FluentElement::Dark}) {
                SCOPED_TRACE(static_cast<int>(theme));
                for (auto* edit : {&reference, input})
                    edit->setProperty("fluentThemeOverride", static_cast<int>(theme));
                for (bool focused : {false, true}) {
                    SCOPED_TRACE(focused);
                    for (const auto widths : {QPair<int, int>{0, 2}, {3, 4}, {0, 0}}) {
                        SCOPED_TRACE(::testing::Message() << widths.first << "/" << widths.second);
                        for (bool frameVisible : {true, false}) {
                            SCOPED_TRACE(frameVisible);
                            for (qreal dpr : {1.0, 1.5, 2.0}) {
                                SCOPED_TRACE(dpr);
                                const auto render = [&](LineEdit* edit) {
                                    edit->setUnfocusedBorderWidth(widths.first);
                                    edit->setFocusedBorderWidth(widths.second);
                                    edit->setFrameVisible(frameVisible);
                                    edit->show();
                                    edit->setFocus(Qt::OtherFocusReason);
                                    if (!focused)
                                        edit->clearFocus();
                                    QApplication::processEvents();
                                    EXPECT_EQ(edit->hasFocus(), focused);
                                    QImage image(qRound(edit->width() * dpr), qRound(edit->height() * dpr),
                                        QImage::Format_ARGB32_Premultiplied);
                                    image.setDevicePixelRatio(dpr);
                                    image.fill(Qt::transparent);
                                    QPainter painter(&image);
                                    edit->render(&painter);
                                    edit->hide();
                                    return image;
                                };
                                const QImage expected = render(&reference);
                                const QImage actual = render(input);
                                const int x = actual.width() / 2;
                                const int top = actual.height() - expected.height();
                                for (int offset = 0; offset < qRound(5 * dpr); ++offset) {
                                    EXPECT_EQ(actual.pixelColor(x, top + offset), expected.pixelColor(x, offset));
                                    EXPECT_EQ(actual.pixelColor(x, actual.height() - 1 - offset),
                                        expected.pixelColor(x, expected.height() - 1 - offset));
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}

TEST_F(LineEditTest, Contract_ClearButtonIsNamedAndUsable)
{
    auto* edit = new LineEdit(window);
    layout->addWidget(edit);
    edit->setText(QStringLiteral("Editable text"));
    window->show();
    QApplication::setActiveWindow(window);
    edit->setFocus();
    QApplication::processEvents();

    auto* clearButton = edit->findChild<Button*>(QStringLiteral("fluentLineEditClearButton"));
    ASSERT_NE(clearButton, nullptr);
    ASSERT_TRUE(clearButton->isVisible());
    EXPECT_EQ(clearButton->size(), QSize(24, 24));
    EXPECT_TRUE(edit->rect().contains(clearButton->geometry()));
    auto* accessible = QAccessible::queryAccessibleInterface(clearButton);
    ASSERT_NE(accessible, nullptr);
    EXPECT_EQ(accessible->text(QAccessible::Name), clearButton->accessibleName());
    EXPECT_FALSE(accessible->text(QAccessible::Name).isEmpty());

    QTest::mouseClick(clearButton, Qt::LeftButton);
    EXPECT_TRUE(edit->text().isEmpty());
    EXPECT_TRUE(edit->hasFocus());
    EXPECT_FALSE(clearButton->isVisible());
}

TEST_F(LineEditTest, ClearButtonOffsetAffectsGeometry)
{
    LineEdit* edit = new LineEdit(window);
    edit->setClearButtonEnabled(true);
    edit->setText("x");
    edit->setFixedSize(200, 40);
    // 在无屏环境下 resizeEvent 可能不会立刻触发，这里用 setter 主动刷新几何
    edit->setClearButtonSize(20);
    edit->setClearButtonOffset(QPoint(12, 5));

    // internal clear button is a fluent::basicinput::Button child
    const auto buttons = edit->findChildren<::fluent::basicinput::Button*>();
    ASSERT_EQ(buttons.size(), 1);
    auto* clearBtn = buttons.first();
    ASSERT_NE(clearBtn, nullptr);

    const int expectedX = edit->width() - 20 - 12;
    const int expectedY = (edit->height() - 20) / 2 + 5;
    EXPECT_EQ(clearBtn->pos(), QPoint(expectedX, expectedY));
}

TEST_F(LineEditTest, VisualCheck)
{
    if (qEnvironmentVariableIsSet("SKIP_VISUAL_TEST")) {
        GTEST_SKIP() << "Set SKIP_VISUAL_TEST=1 to skip visual tests";
    }

    using Edge = AnchorLayout::Edge;

    Label* header = new Label("LineEdit (single-line):", window);
    header->anchors()->top = {window, Edge::Top, 30};
    header->anchors()->left = {window, Edge::Left, 40};
    layout->addWidget(header);

    // 默认样式
    LineEdit* edit = new LineEdit(window);
    edit->setPlaceholderText("Default LineEdit...");
    edit->setText("Sample text");
    edit->setContentMargins(QMargins(8, 4, 8, 4));
    edit->anchors()->top = {header, Edge::Bottom, 8};
    edit->anchors()->left = {window, Edge::Left, 40};
    edit->anchors()->right = {window, Edge::Right, -40};
    layout->addWidget(edit);

    // 带自定义 clearButtonOffset 的例子
    Label* offsetHeader = new Label("With custom clearButtonOffset:", window);
    offsetHeader->anchors()->top = {edit, Edge::Bottom, 20};
    offsetHeader->anchors()->left = {window, Edge::Left, 40};
    layout->addWidget(offsetHeader);

    LineEdit* offsetEdit = new LineEdit(window);
    offsetEdit->setPlaceholderText("Clear button offset (x=12, y=4)...");
    offsetEdit->setText("Offset clear button");
    offsetEdit->setContentMargins(QMargins(8, 4, 8, 4));
    offsetEdit->setClearButtonOffset(QPoint(12, 4));
    offsetEdit->anchors()->top = {offsetHeader, Edge::Bottom, 8};
    offsetEdit->anchors()->left = {window, Edge::Left, 40};
    offsetEdit->anchors()->right = {window, Edge::Right, -40};
    layout->addWidget(offsetEdit);

    // 带自定义边框粗细的例子
    Label* borderHeader = new Label("Custom focused/unfocused border widths:", window);
    borderHeader->anchors()->top = {offsetEdit, Edge::Bottom, 20};
    borderHeader->anchors()->left = {window, Edge::Left, 40};
    layout->addWidget(borderHeader);

    LineEdit* borderEdit = new LineEdit(window);
    borderEdit->setPlaceholderText("Focused=3px, Unfocused=2px...");
    borderEdit->setText("Border thickness demo");
    borderEdit->setContentMargins(QMargins(8, 4, 8, 4));
    borderEdit->setFocusedBorderWidth(3);
    borderEdit->setUnfocusedBorderWidth(2);
    borderEdit->anchors()->top = {borderHeader, Edge::Bottom, 8};
    borderEdit->anchors()->left = {window, Edge::Left, 40};
    borderEdit->anchors()->right = {window, Edge::Right, -40};
    layout->addWidget(borderEdit);

    Button* themeBtn = new Button("Switch Theme", window);
    themeBtn->setFluentStyle(Button::Accent);
    themeBtn->setFixedSize(120, 32);
    themeBtn->anchors()->bottom = {window, Edge::Bottom, -30};
    themeBtn->anchors()->right = {window, Edge::Right, -30};
    layout->addWidget(themeBtn);

    QObject::connect(themeBtn, &Button::clicked, []() {
        fluent::FluentElement::setTheme(fluent::FluentElement::currentTheme() == fluent::FluentElement::Light
                ? fluent::FluentElement::Dark
                : fluent::FluentElement::Light);
    });

    window->show();
    if (tests::support::shouldCaptureVisualSnapshot()) {
        ASSERT_TRUE(tests::support::captureVisualSnapshot(window));
        return;
    }

    qApp->exec();
}
