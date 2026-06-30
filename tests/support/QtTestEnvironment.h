#ifndef FLUENT_QT_QT_TEST_ENVIRONMENT_H
#define FLUENT_QT_QT_TEST_ENVIRONMENT_H

#include <QSize>
#include <QString>

#include <gtest/gtest.h>

class QWidget;

namespace tests::support {

enum class VisualSnapshotTheme {
	Light,
	Dark
};

struct VisualSnapshotOptions {
	QSize windowSize;
	QString variant;
	VisualSnapshotTheme theme = VisualSnapshotTheme::Light;
};

void configureOffscreenPlatformForAutomation();
void initializeQtTestEnvironment();
bool shouldSkipVisualTest();
bool isVisualSnapshotMode();
bool shouldCaptureVisualSnapshot();
QString visualSnapshotDirectory();
QString visualSnapshotFilePath(const QString& variant = QString());
::testing::AssertionResult captureVisualSnapshot(QWidget* window,
												 const VisualSnapshotOptions& options = VisualSnapshotOptions());

} // namespace tests::support

#endif // FLUENT_QT_QT_TEST_ENVIRONMENT_H
