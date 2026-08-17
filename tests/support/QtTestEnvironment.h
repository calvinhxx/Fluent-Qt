#ifndef FLUENT_QT_QT_TEST_ENVIRONMENT_H
#define FLUENT_QT_QT_TEST_ENVIRONMENT_H

#include <QImage>
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
	QString focusObjectName;
	VisualSnapshotTheme theme = VisualSnapshotTheme::Light;
};

void configureOffscreenPlatformForAutomation();
void initializeQtTestEnvironment();
bool shouldSkipVisualTest();
// True when running under a headless platform plugin (offscreen/minimal) that
// cannot faithfully deliver synthetic pointer/keyboard input or show native
// popups — e.g. CI. Tests that drive drag-reorder, menu popups, or window
// activation should GTEST_SKIP() on these platforms so they keep running on
// real desktops but no longer fail headless CI.
bool isHeadlessPlatform();
bool isVisualSnapshotMode();
bool isVisualCompareMode();
bool shouldUpdateVisualBaseline();
bool shouldCaptureVisualSnapshot();
// Opt-in representative Light/Dark/RTL gate. Requires snapshot or compare mode
// and must not be skipped. Headless platforms skip at the test site.
bool shouldRunVisualGate();
// Checked-in visual baselines are approved only on the documented macOS arm64
// desktop capture stack. Other hosts may capture ad-hoc snapshots, but must not
// compare or update this baseline set.
bool isVisualGateApprovalHost();
QString visualSnapshotDirectory();
QString visualSnapshotFilePath(const QString& variant = QString());
QString visualBaselineDirectory();
QString visualBaselineFilePath(const QString& variant = QString());
::testing::AssertionResult compareVisualImages(const QImage& actual,
											   const QImage& expected);
::testing::AssertionResult compareVisualSnapshotToBaseline(
	const QString& actualPath,
	const QString& variant = QString());
::testing::AssertionResult captureVisualSnapshot(QWidget* window,
												 const VisualSnapshotOptions& options = VisualSnapshotOptions());

} // namespace tests::support

#endif // FLUENT_QT_QT_TEST_ENVIRONMENT_H
