// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "helperscenario.h"

#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QGuiApplication>
#include <QTextStream>

#include <cstdio>
#include <cstdlib>

namespace {
constexpr quint32 Normal = 0;
constexpr quint32 Show = 1;

HelperScenarioResult runScenario()
{
    HelperScenario scenario;
#ifdef TL_PROTOCOL_TEST_WRONG_HELPER_EXPECTED
    return scenario.run(Normal);
#else
    return scenario.run(Show);
#endif
}

[[noreturn]] void exitTestProcess(int exitCode)
{
    std::fflush(nullptr);
    std::_Exit(exitCode);
}
}

int main(int argc, char **argv)
{
    QGuiApplication application(argc, argv);
    QCoreApplication::setApplicationName(
        QStringLiteral("test-protocol-window-management-helper"));

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

    const HelperScenarioResult result = runScenario();

    if (!writeHelperSummary(result, parser.value(reportDirectoryOption))) {
        QTextStream error(stderr);
        error << "Unable to write summary.json\n";
        error.flush();
        exitTestProcess(2);
    }

    QTextStream output(result.passed ? stdout : stderr);
    if (parser.isSet(verboseOption) || !result.passed) {
        for (auto it = result.checks.cbegin(); it != result.checks.cend(); ++it)
            output << (it.value() ? "[PASS] " : "[FAIL] ") << it.key() << '\n';
        if (!result.passed)
            output << "FAILURE CATEGORY: " << result.failureCategory << '\n';
        output << "RESULT: " << (result.passed ? "PASS" : "FAIL") << '\n';
    } else {
        output << "CASE window-management.helper-set-desktop-show: "
               << (result.passed ? "PASS" : "FAIL")
               << " (" << result.elapsedMs << " ms)\n";
    }
    output.flush();

    // Helper constructs process-lifetime services such as DConfig. All objects
    // owned by this Wayland scenario have already been destroyed and checked,
    // so do not make this focused test depend on unrelated static teardown.
    exitTestProcess(result.passed ? 0 : 1);
}
