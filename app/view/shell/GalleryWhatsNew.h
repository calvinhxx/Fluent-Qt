#ifndef GALLERYWHATSNEW_H
#define GALLERYWHATSNEW_H

#include "components/dialogs_flyouts/Dialog.h"

class QHBoxLayout;
class QVBoxLayout;

namespace fluent::gallery {

class GalleryWhatsNew final : public dialogs_flyouts::Dialog {
public:
    explicit GalleryWhatsNew(QWidget* trigger);
    static QString announcementGlyph();

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
    void showEvent(QShowEvent* event) override;

private:
    void fitWindow();
    QPointer<QWidget> m_host;
    QHBoxLayout* m_header = nullptr;
    QVBoxLayout* m_entries = nullptr;
};
} // namespace fluent::gallery
#endif
