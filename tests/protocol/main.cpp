// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wirescenario.h"

#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QGuiApplication>
#include <QTextStream>

namespace {
constexpr quint32 Show = 1;
constexpr quint32 MissingEventExpectation = 2;
}

int main(int argc, char **argv)
{
    QGuiApplication application(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("test-protocol-window-management-wire"));

    QCommandLineParser parser;
    parser.addHelpOption();
    const QCommandLineOption verboseOption(
        QStringLiteral("verbose"),
        QStringLiteral("Print every structured check."));
    const QCommandLineOption reportDirectoryOption(
        QStringLiteral("report-dir"),
        QStringLiteral("Directory in which summary.json is written."),
        QStringLiteral("directory"),
        QStringLiteral(TL_PROTOCOL_TEST_DEFAULT_REPORT_DIR));
    parser.addOption(verboseOption);
    parser.addOption(reportDirectoryOption);
    parser.process(application);

    WireScenario scenario;
#ifdef TL_PROTOCOL_TEST_WRONG_EXPECTED
    const WireScenarioResult result = scenario.run(MissingEventExpectation);
#else
    const WireScenarioResult result = scenario.run(Show);
#endif

    if (!writeSummary(result, parser.value(reportDirectoryOption))) {
        QTextStream(stderr) << "Unable to write summary.json\n";
        return 2;
    }

    QTextStream output(result.passed ? stdout : stderr);
    if (parser.isSet(verboseOption) || !result.passed) {
        for (auto it = result.checks.cbegin(); it != result.checks.cend(); ++it)
            output << (it.value() ? "[PASS] " : "[FAIL] ") << it.key() << '\n';
        if (!result.passed)
            output << "FAILURE CATEGORY: " << result.failureCategory << '\n';
        output << "RESULT: " << (result.passed ? "PASS" : "FAIL") << '\n';
    } else {
        output << "CASE window-management.set-desktop-show: "
               << (result.passed ? "PASS" : "FAIL")
               << " (" << result.elapsedMs << " ms)\n";
    }
    return result.passed ? 0 : 1;
}
