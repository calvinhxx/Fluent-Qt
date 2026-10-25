#ifndef GALLERY_FILEBROWSERSAMPLE_H
#define GALLERY_FILEBROWSERSAMPLE_H

#include <FluentQt/FluentQt.h>

#include <QAction>
#include <QHBoxLayout>
#include <QIcon>
#include <QJsonObject>
#include <QKeySequence>
#include <QPixmap>
#include <QSignalBlocker>
#include <QSortFilterProxyModel>
#include <QStandardItemModel>
#include <QTextBlock>
#include <QVBoxLayout>

namespace fluent::gallery {

// The model and drafts belong to the example; ListDetailView only arranges panes.
// zh_CN: 模型和草稿由示例持有，ListDetailView 只负责排列内容区。
class FileBrowserSample final : public layout::Card {
    using Button = basicinput::Button;
    using Label = textfields::Label;
    enum Role { Draft = Qt::UserRole, Modified, Saved, Folder };

public:
    explicit FileBrowserSample(QWidget* parent = nullptr) : Card(parent)
    {
        setObjectName(QStringLiteral("fileBrowserSample"));
        setBorderVisible(false);
        setMaximumWidth(960);
        setMinimumHeight(644);
        auto* body = column(this);
        body->setSpacing(::Spacing::Medium);
        body->setSizeConstraint(QLayout::SetNoConstraint);
        m_surface = new layout::Card(this);
        m_surface->setObjectName(QStringLiteral("fileBrowserSurface"));
        m_surface->setFixedHeight(600);
        m_surface->setThemeOverrides({{QStringLiteral("radius"), QJsonObject{{QStringLiteral("control"), 8}}}});
        auto* surface = column(m_surface, 1);
        surface->setSizeConstraint(QLayout::SetNoConstraint);
        body->addWidget(m_surface);

        auto* toolbar = row(m_surface, 52, ::Spacing::Medium);
        m_breadcrumb = new QWidget(m_surface);
        auto* path = new QHBoxLayout(m_breadcrumb);
        path->setContentsMargins(0, 0, 0, 0);
        path->setSpacing(::Spacing::Medium);
        path->addWidget(new FontIcon(QStringLiteral("ic_fluent_folder_20_regular"), m_breadcrumb));
        path->addWidget(label(QStringLiteral("Workspace"), Typography::FontRole::Body, true));
        auto* chevron = new FontIcon(QStringLiteral("ic_fluent_chevron_right_20_regular"), m_breadcrumb);
        chevron->setIconSize(Typography::IconSize::Compact);
        path->addWidget(chevron);
        toolbar->addWidget(m_breadcrumb);
        m_folderTitle = label(QStringLiteral("Documents"), Typography::FontRole::BodyStrong);
        toolbar->addWidget(m_folderTitle);
        m_back = button(QStringLiteral("Files"), "fileBrowserBack", Button::Subtle);
        m_back->setIconGlyph(Typography::Icons::Back);
        m_back->setAccessibleName(QStringLiteral("Back to files"));
        toolbar->addWidget(m_back);
        toolbar->addStretch();
        m_wideSearch = search("fileBrowserSearch");
        m_wideSearch->setFixedWidth(204);
        toolbar->addWidget(m_wideSearch);
        m_new = button(QStringLiteral("New file"), "fileBrowserNew");
        m_new->setIconGlyph(Typography::Icons::Add);
        m_new->setAccessibleName(QStringLiteral("New file"));
        toolbar->addWidget(m_new);
        m_more = button(QString(), "fileBrowserMore", Button::Subtle);
        m_more->setIconGlyph(Typography::Icons::More);
        m_more->setAccessibleName(QStringLiteral("File options"));
        m_more->setToolTip(m_more->accessibleName());
        toolbar->addWidget(m_more);
        m_compactSave = button(QStringLiteral("Save"), "fileBrowserCompactSave", Button::Accent);
        toolbar->addWidget(m_compactSave);
        surface->addWidget(toolbar->parentWidget());
        surface->addWidget(new layout::Divider(m_surface));

        m_view = new layout::ListDetailView(m_surface);
        m_view->setObjectName(QStringLiteral("fileBrowserLayout"));
        m_view->setListPaneWidth(320);
        surface->addWidget(m_view, 1);
        auto* list = column(m_view->listPane());
        m_searchRow = row(m_view->listPane(), 56, ::Spacing::Medium)->parentWidget();
        m_narrowSearch = search("fileBrowserCompactSearch");
        m_searchRow->layout()->addWidget(m_narrowSearch);
        list->addWidget(m_searchRow);
        auto* headings = row(m_view->listPane(), 36, 0);
        headings->setContentsMargins(48, 0, 12, 0);
        headings->addWidget(label(QStringLiteral("Name"), Typography::FontRole::Caption, true));
        headings->addStretch();
        headings->addWidget(label(QStringLiteral("Modified"), Typography::FontRole::Caption, true));
        list->addWidget(headings->parentWidget());
        m_list = new collections::ListView(m_view->listPane());
        m_list->setObjectName(QStringLiteral("fileBrowserList"));
        m_list->setAccessibleName(QStringLiteral("Documents"));
        m_list->setBorderVisible(false);
        m_list->setScrollChainingEnabled(false);
        m_list->setTrailingTextRole(Modified);
        m_list->setIconSize(QSize(20, 20));
        m_list->setSpacing(0);
        m_list->setPlaceholderText(QStringLiteral("No matching files"));
        list->addWidget(m_list, 1);
        list->addWidget(new layout::Divider(m_view->listPane()));
        auto* listFooter = row(m_view->listPane(), 28, ::Spacing::Standard);
        m_count = label(QString(), Typography::FontRole::Caption, true);
        listFooter->addWidget(m_count);
        list->addWidget(listFooter->parentWidget());

        auto* detail = column(m_view->detailPane());
        auto* heading = row(m_view->detailPane(), 64, ::Spacing::Large);
        auto* identity = new QVBoxLayout;
        identity->setSpacing(::Spacing::XSmall);
        identity->setAlignment(Qt::AlignVCenter);
        m_title = label(QString(), Typography::FontRole::BodyStrong);
        m_title->setObjectName(QStringLiteral("fileBrowserTitle"));
        m_title->setMinimumWidth(0);
        identity->addWidget(m_title);
        auto* state = new QHBoxLayout;
        state->setSpacing(::Spacing::XSmall);
        m_dirty = new status_info::InfoBadge(this);
        m_dirty->setDisplayMode(status_info::InfoBadge::InfoBadgeDisplayMode::Dot);
        m_dirty->setStatus(status_info::InfoBadge::InfoBadgeStatus::Caution);
        m_dirty->setBeaconDiameter(5);
        m_dirty->setFixedSize(5, 5);
        m_dirty->setAccessibleName(QStringLiteral("Unsaved changes"));
        state->addWidget(m_dirty);
        m_status = label(QString(), Typography::FontRole::Caption, true);
        m_status->setObjectName(QStringLiteral("fileBrowserStatus"));
        state->addWidget(m_status);
        state->addStretch();
        identity->addLayout(state);
        heading->addLayout(identity, 1);
        m_save = button(QStringLiteral("Save"), "fileBrowserSave", Button::Accent);
        heading->addWidget(m_save);
        detail->addWidget(heading->parentWidget());
        detail->addWidget(new layout::Divider(m_view->detailPane()));
        m_editor = new textfields::TextEdit(m_view->detailPane());
        m_editor->setObjectName(QStringLiteral("fileBrowserEditor"));
        m_editor->setAccessibleName(QStringLiteral("File contents"));
        m_editor->setAutoResizeEnabled(false);
        m_editor->setFrameVisible(false);
        m_editor->setContentMargins(QMargins(24, 24, 24, 24));
        m_editor->setLineHeight(::Spacing::ControlHeight::Small);
        m_editor->setScrollChainingEnabled(false);
        detail->addWidget(m_editor, 1);
        detail->addWidget(new layout::Divider(m_view->detailPane()));
        auto* editorFooter = row(m_view->detailPane(), 28, ::Spacing::Standard);
        m_type = label(QString(), Typography::FontRole::Caption, true);
        m_position = label(QString(), Typography::FontRole::Caption, true);
        editorFooter->addWidget(m_type);
        editorFooter->addStretch();
        editorFooter->addWidget(m_position);
        detail->addWidget(editorFooter->parentWidget());

        auto* narrow = button(QStringLiteral("Narrow preview"), "fileBrowserNarrow", Button::Subtle);
        narrow->setCheckable(true);
        body->addWidget(narrow, 0, Qt::AlignLeft);
        connect(narrow, &Button::toggled, this, [this](bool checked) {
            if (checked)
                m_previewWidth = width();
            setMaximumWidth(checked ? 400 : 960);
            resize(checked ? 400 : m_previewWidth, height());
        });
        m_documentIcon = glyphIcon(QStringLiteral("ic_fluent_document_text_20_regular"), 20);
        m_folderIcon = glyphIcon(QStringLiteral("ic_fluent_folder_20_regular"), 20);
        m_model = new QStandardItemModel(this);
        const QStringList names{QStringLiteral("Design assets"), QStringLiteral("getting-started.md"),
            QStringLiteral("layout-notes.md"), QStringLiteral("architecture.md"), QStringLiteral("keyboard.md"),
            QStringLiteral("roadmap.md"), QStringLiteral("changelog.md"), QStringLiteral("theme-tokens.json"),
            QStringLiteral("contributing.md"), QStringLiteral("license.txt"), QStringLiteral("release-notes.md"),
            QStringLiteral("readme.md")};
        const QStringList dates{QStringLiteral("Oct 18"), QStringLiteral("11:20"), QStringLiteral("10:42"),
            QStringLiteral("Yesterday"), QStringLiteral("Oct 18"), QStringLiteral("Oct 18"), QStringLiteral("Oct 17"),
            QStringLiteral("Oct 17"), QStringLiteral("Oct 16"), QStringLiteral("Oct 15"), QStringLiteral("Oct 15"),
            QStringLiteral("Oct 14")};
        for (int i = 0; i < names.size(); ++i)
            addFile(names[i], dates[i],
                i == 2 ? layoutNotes() : QStringLiteral("# %1\n\nNotes for this workspace.\n").arg(names[i]), i == 0);
        m_model->item(0)->setData(
            QStringLiteral("Design assets\n\nFolder contents\n\napp-icon.svg\ncolor-tokens.json\nREADME.md"), Draft);
        m_model->item(2)->setData(layoutNotes().section(QStringLiteral("Next:"), 0, 0), Saved);
        for (int i = 1; i <= 20; ++i)
            addFile(QStringLiteral("meeting-notes-%1.md").arg(i, 2, 10, QLatin1Char('0')), QStringLiteral("Oct 12"),
                QStringLiteral("# Meeting notes %1\n\nDecisions\n\n").arg(i)
                    + QStringLiteral("Keep the next step small and reviewable.\n").repeated(40));
        m_filter = new QSortFilterProxyModel(this);
        m_filter->setSourceModel(m_model);
        m_filter->setFilterCaseSensitivity(Qt::CaseInsensitive);
        m_filter->setFilterKeyColumn(0);
        m_list->setModel(m_filter);
        connect(m_list->selectionModel(), &QItemSelectionModel::currentChanged, this, [this](const QModelIndex& index) {
            selectFile(index);
        });
        connect(m_list, &collections::ListView::clicked, this, [this] { openFile(); });
        connect(m_list, &collections::ListView::activated, this, [this] { openFile(); });
        auto* open = new QAction(m_list);
        open->setShortcuts({QKeySequence(Qt::Key_Return), QKeySequence(Qt::Key_Enter)});
        open->setShortcutContext(Qt::WidgetShortcut);
        m_list->addAction(open);
        connect(open, &QAction::triggered, this, [this] { openFile(); });
        connect(m_back, &Button::clicked, this, [this] {
            m_view->showList();
            m_list->setFocus(Qt::OtherFocusReason);
        });
        connect(m_view, &layout::ListDetailView::compactChanged, this, [this] { updateChrome(); });
        connect(m_view, &layout::ListDetailView::currentPaneChanged, this, [this] { updateChrome(); });
        connect(m_wideSearch, &textfields::LineEdit::textChanged, this, [this](const QString& text) {
            filterFiles(text);
        });
        connect(m_narrowSearch, &textfields::LineEdit::textChanged, this, [this](const QString& text) {
            filterFiles(text);
        });
        connect(m_editor, &textfields::TextEdit::textChanged, this, [this] {
            if (m_current.isValid()) {
                m_model->setData(m_current, m_editor->toPlainText(), Draft);
                updateState();
                m_view->showDetail();
            }
        });
        connect(m_editor, &textfields::TextEdit::cursorPositionChanged, this, [this] { updatePosition(); });
        auto* save = new QAction(this);
        save->setShortcut(QKeySequence::Save);
        save->setShortcutContext(Qt::WidgetWithChildrenShortcut);
        addAction(save);
        connect(save, &QAction::triggered, this, [this] { saveFile(); });
        connect(m_save, &Button::clicked, save, &QAction::trigger);
        connect(m_compactSave, &Button::clicked, save, &QAction::trigger);
        connect(m_new, &Button::clicked, this, [this] {
            filterFiles(QString());
            const int next = ++m_newCount;
            addFile(QStringLiteral("untitled-%1.md").arg(next), QStringLiteral("Just now"),
                QStringLiteral("# Untitled\n\n"));
            m_model->item(m_model->rowCount() - 1)->setData(QString(), Saved);
            m_list->setCurrentIndex(m_filter->mapFromSource(m_model->index(m_model->rowCount() - 1, 0)));
            openFile();
        });
        auto* menu = new menus_toolbars::FluentMenu(QStringLiteral("File options"), this);
        connect(menu->addAction(QStringLiteral("Show current file")), &QAction::triggered, this, [this] {
            filterFiles(QString());
            m_list->scrollTo(m_list->currentIndex());
        });
        connect(menu->addAction(QStringLiteral("Clear search")), &QAction::triggered, m_wideSearch, &QLineEdit::clear);
        connect(m_more, &Button::clicked, menu, [this, menu] {
            menu->popup(m_more->mapToGlobal(QPoint(0, m_more->height())));
        });
        m_list->setCurrentIndex(m_filter->index(2, 0));
        m_view->showList();
        onThemeUpdated();
        updateChrome();
    }

    void onThemeUpdated() override
    {
        Card::onThemeUpdated();
        if (!m_model)
            return;
        const auto colors = themeColors();
        m_documentIcon = glyphIcon(QStringLiteral("ic_fluent_document_text_20_regular"), 20);
        m_folderIcon = glyphIcon(QStringLiteral("ic_fluent_folder_20_regular"), 20);
        for (int i = 0; i < m_model->rowCount(); ++i)
            m_model->item(i)->setIcon(m_model->item(i)->data(Folder).toBool() ? m_folderIcon : m_documentIcon);
        const QColor secondary = colors.textSecondary;
        const QString searchText = QString::asprintf("#%02x%02x%02x%02x", secondary.red(), secondary.green(),
            secondary.blue(), secondary.alpha());
        const QString mode = effectiveTheme() == Dark ? QStringLiteral("dark")
            : effectiveTheme() == HighContrast
            ? QStringLiteral("contrast")
            : QStringLiteral("light");
        const QJsonObject searchTheme{{mode, QJsonObject{{QStringLiteral("textTertiary"), searchText}}}};
        m_wideSearch->setThemeOverrides(searchTheme);
        m_narrowSearch->setThemeOverrides(searchTheme);
    }

private:
    static QVBoxLayout* column(QWidget* parent, int margin = 0)
    {
        auto* layout = new QVBoxLayout(parent);
        layout->setContentsMargins(margin, margin, margin, margin);
        layout->setSpacing(0);
        return layout;
    }
    static QHBoxLayout* row(QWidget* parent, int height, int inset)
    {
        auto* widget = new QWidget(parent);
        widget->setFixedHeight(height);
        auto* layout = new QHBoxLayout(widget);
        layout->setContentsMargins(inset, 0, inset, 0);
        layout->setSpacing(::Spacing::Small);
        return layout;
    }
    Label* label(const QString& text, Typography::FontRole role, bool secondary = false)
    {
        auto* result = new Label(text, this);
        result->setFluentTypography(role);
        result->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
        if (secondary)
            result->setTextColorRole(Label::TextColorRole::Secondary);
        return result;
    }
    Button* button(const QString& text, const char* name, Button::ButtonStyle style = Button::Standard)
    {
        auto* result = new Button(text, this);
        result->setObjectName(QString::fromLatin1(name));
        result->setFluentStyle(style);
        return result;
    }
    textfields::LineEdit* search(const char* name)
    {
        auto* result = new textfields::LineEdit(this);
        result->setObjectName(QString::fromLatin1(name));
        result->setPlaceholderText(QStringLiteral("Search files…"));
        result->setAccessibleName(QStringLiteral("Search files"));
        auto* icon = new FontIcon(QStringLiteral("ic_fluent_search_20_regular"), result);
        icon->setIconSize(16);
        icon->setFixedSize(16, 16);
        icon->setAttribute(Qt::WA_TransparentForMouseEvents);
        auto* layout = new QHBoxLayout(result);
        layout->setContentsMargins(::Spacing::Small, 0, ::Spacing::Small, 0);
        layout->addWidget(icon, 0, Qt::AlignVCenter);
        layout->addStretch();
        auto margins = result->contentMargins();
        margins.setLeft(32);
        result->setContentMargins(margins);
        return result;
    }
    QIcon glyphIcon(const QString& glyph, int size)
    {
        FontIcon icon(glyph, this);
        icon.setIconSize(size);
        icon.resize(size, size);
        QPixmap pixmap(QSize(size, size) * devicePixelRatioF());
        pixmap.setDevicePixelRatio(devicePixelRatioF());
        pixmap.fill(Qt::transparent);
        icon.render(&pixmap, QPoint(), QRegion(), QWidget::DrawChildren);
        return QIcon(pixmap);
    }
    void addFile(const QString& name, const QString& modified, const QString& contents, bool folder = false)
    {
        auto* item = new QStandardItem(name);
        item->setEditable(false);
        item->setSizeHint(QSize(0, ::Spacing::ControlHeight::Large));
        item->setData(contents, Draft);
        item->setData(contents, Saved);
        item->setData(modified, Modified);
        item->setData(folder, Folder);
        item->setData(QStringLiteral("%1, modified %2").arg(name, modified), Qt::AccessibleTextRole);
        item->setIcon(folder ? m_folderIcon : m_documentIcon);
        m_model->appendRow(item);
    }
    void selectFile(const QModelIndex& proxy)
    {
        if (m_filtering || !proxy.isValid())
            return;
        const QModelIndex index = m_filter->mapToSource(proxy);
        if (index == m_current)
            return;
        m_current = index;
        m_title->setText(index.data().toString());
        const QSignalBlocker blocker(m_editor);
        m_editor->setPlainText(index.data(Draft).toString());
        m_editor->setReadOnly(index.data(Folder).toBool());
        updateState();
        updatePosition();
        if (!m_view->isCompact())
            m_view->showDetail();
    }
    void openFile()
    {
        if (!m_list->currentIndex().isValid())
            return;
        m_view->showDetail();
        m_editor->setFocus();
    }
    void filterFiles(const QString& text)
    {
        const QSignalBlocker wide(m_wideSearch), narrow(m_narrowSearch);
        if (m_wideSearch->text() != text)
            m_wideSearch->setText(text);
        if (m_narrowSearch->text() != text)
            m_narrowSearch->setText(text);
        m_filtering = true;
        m_filter->setFilterFixedString(text);
        m_list->setCurrentIndex(m_filter->mapFromSource(m_current));
        m_filtering = false;
        updateState();
    }
    void saveFile()
    {
        if (!m_current.isValid() || m_current.data(Folder).toBool())
            return;
        m_model->setData(m_current, m_current.data(Draft), Saved);
        m_model->setData(m_current, QStringLiteral("Just now"), Modified);
        m_model->setData(m_current, QStringLiteral("%1, modified Just now").arg(m_current.data().toString()),
            Qt::AccessibleTextRole);
        updateState();
    }
    void updateState()
    {
        const bool folder = m_current.data(Folder).toBool();
        const bool dirty = !folder && m_current.data(Draft) != m_current.data(Saved);
        m_dirty->setVisible(dirty);
        m_status->setText(folder ? QStringLiteral("Folder · 3 items")
                : dirty          ? QStringLiteral("Unsaved changes")
                                 : QStringLiteral("Saved in memory"));
        m_save->setEnabled(dirty);
        m_compactSave->setEnabled(dirty);
        m_type->setText(folder ? QStringLiteral("Folder")
                : m_current.data().toString().endsWith(QStringLiteral(".md"))
                ? QStringLiteral("Markdown · UTF-8")
                : QStringLiteral("Plain text · UTF-8"));
        m_count->setText(QStringLiteral("%1 items%2")
                .arg(m_filter->rowCount())
                .arg(m_list->currentIndex().isValid() ? QStringLiteral(" · 1 selected") : QString()));
    }
    void updatePosition()
    {
        const QTextCursor cursor = m_editor->textCursor();
        m_position->setText(
            QStringLiteral("Ln %1, Col %2").arg(cursor.blockNumber() + 1).arg(cursor.positionInBlock() + 1));
    }
    void updateChrome()
    {
        const bool compact = m_view->isCompact();
        const bool detail = compact && m_view->currentPane() == layout::ListDetailView::Detail;
        m_breadcrumb->setVisible(!compact);
        m_folderTitle->setVisible(!detail);
        m_back->setVisible(detail);
        m_wideSearch->setVisible(!compact);
        m_new->setVisible(!detail);
        m_new->setText(compact ? QString() : QStringLiteral("New file"));
        m_more->setVisible(compact && !detail);
        m_compactSave->setVisible(detail);
        m_searchRow->setVisible(compact);
        m_save->setVisible(!compact);
    }
    static QString layoutNotes()
    {
        return QStringLiteral(
            "# Layout notes\n\nKeep the browser quiet. Let the document lead.\n\n"
            "## Wide windows\n- List on the left, content on the right.\n- One divider. No cards inside cards.\n\n"
            "## Small windows\n- Open a file to focus on its contents.\n- Return to the same place in the list.\n\n"
            "Next: refine the empty state.");
    }

    layout::Card* m_surface = nullptr;
    layout::ListDetailView* m_view = nullptr;
    collections::ListView* m_list = nullptr;
    QStandardItemModel* m_model = nullptr;
    QSortFilterProxyModel* m_filter = nullptr;
    QPersistentModelIndex m_current;
    textfields::TextEdit* m_editor = nullptr;
    textfields::LineEdit *m_wideSearch = nullptr, *m_narrowSearch = nullptr;
    QWidget *m_breadcrumb = nullptr, *m_searchRow = nullptr;
    Label *m_folderTitle = nullptr, *m_title = nullptr, *m_status = nullptr, *m_count = nullptr, *m_type = nullptr,
          *m_position = nullptr;
    Button *m_back = nullptr, *m_new = nullptr, *m_more = nullptr, *m_save = nullptr, *m_compactSave = nullptr;
    status_info::InfoBadge* m_dirty = nullptr;
    QIcon m_documentIcon, m_folderIcon;
    bool m_filtering = false;
    int m_newCount = 0;
    int m_previewWidth = 960;
};

} // namespace fluent::gallery
#endif
