#include <FluentQt/FluentQt.h>

#include <QApplication>
#include <QFont>
#include <QLabel>
#include <QVBoxLayout>
#include <QWidget>

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);

    QWidget window;
    window.setWindowTitle(QStringLiteral("Fluent-Qt Hello World"));
    window.resize(420, 220);

    auto* layout = new QVBoxLayout(&window);
    layout->setContentsMargins(32, 32, 32, 32);
    layout->setSpacing(16);

    auto* title = new QLabel(QStringLiteral("Hello Fluent-Qt"), &window);
    QFont titleFont = title->font();
    titleFont.setPointSize(20);
    titleFont.setBold(true);
    title->setFont(titleFont);

    auto* status = new QLabel(
        QStringLiteral("This app links FluentQt::FluentQt from a standalone Qt project."),
        &window);
    status->setWordWrap(true);

    auto* button = new fluent::basicinput::Button(QStringLiteral("Accent button"), &window);
    button->setFluentStyle(fluent::basicinput::Button::Accent);

    QObject::connect(button, &fluent::basicinput::Button::clicked, status, [status]() {
        status->setText(QStringLiteral("Button clicked. Fluent-Qt is linked and running."));
    });

    layout->addWidget(title);
    layout->addWidget(status);
    layout->addWidget(button, 0, Qt::AlignLeft);
    layout->addStretch();

    window.show();
    return app.exec();
}
