// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "protocoljsoncase.h"
#include "protocoljsonscenario.h"

#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QDir>
#include <QGuiApplication>
#include <QTextStream>

int main(int argc, char **argv)
{
    QGuiApplication application(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("treeland-protocol-test-runner"));

    QCommandLineParser parser;
    parser.addHelpOption();
    const QCommandLineOption inputOption(
        QStringLiteral("input"), QStringLiteral("Input JSON file."), QStringLiteral("path"));
    const QCommandLineOption expectedOption(
        QStringLiteral("expected"), QStringLiteral("Expected JSON file."), QStringLiteral("path"));
    const QCommandLineOption metadataOption(
        QStringLiteral("metadata"),
        QStringLiteral("Generated adapter metadata JSON."),
        QStringLiteral("path"),
        QStringLiteral(TL_PROTOCOL_TEST_METADATA));
    const QCommandLineOption xmlOption(
        QStringLiteral("protocol-xml"),
        QStringLiteral("Protocol XML used to validate provenance."),
        QStringLiteral("path"),
        QStringLiteral(TL_PROTOCOL_TEST_XML));
    const QCommandLineOption actualOption(
        QStringLiteral("dump-actual"),
        QStringLiteral("Path for the owning normalized actual JSON."),
        QStringLiteral("path"));
    const QCommandLineOption reportOption(
        QStringLiteral("report-dir"),
        QStringLiteral("Directory for summary.json."),
        QStringLiteral("directory"),
        QStringLiteral(TL_PROTOCOL_TEST_DEFAULT_REPORT_DIR));
    const QCommandLineOption verboseOption(
        QStringLiteral("verbose"), QStringLiteral("Print every structured check."));
    parser.addOptions({ inputOption,
                        expectedOption,
                        metadataOption,
                        xmlOption,
                        actualOption,
                        reportOption,
                        verboseOption });
    parser.process(application);

    const QString reportDirectory = parser.value(reportOption);
    const QString actualPath = parser.isSet(actualOption)
        ? parser.value(actualOption)
        : QDir(reportDirectory).filePath(QStringLiteral("actual.json"));

    ProtocolJsonCase testCase;
    ProtocolJsonValidationError validationError;
    ProtocolJsonRunResult result;
    if (!loadProtocolJsonCase(parser.value(inputOption),
                              parser.value(expectedOption),
                              parser.value(metadataOption),
                              parser.value(xmlOption),
                              testCase,
                              validationError)) {
        result = validationFailureResult(testCase.input.value(QStringLiteral("case")).toString(),
                                         validationError);
    } else {
        const QString reviewStatus =
            testCase.expected.value(QStringLiteral("expectation_source"))
                .toObject()
                .value(QStringLiteral("review_status"))
                .toString();
        if (reviewStatus == QStringLiteral("candidate"))
            QTextStream(stderr) << "WARNING: expected data is an unreviewed candidate\n";
        result = runProtocolJsonCase(testCase);
    }

    if (!writeProtocolJsonArtifacts(result, actualPath, reportDirectory)) {
        QTextStream(stderr) << "Unable to write protocol test artifacts\n";
        return 2;
    }

    QTextStream output(result.passed ? stdout : stderr);
    if (parser.isSet(verboseOption) || !result.passed) {
        for (auto check = result.checks.cbegin(); check != result.checks.cend(); ++check)
            output << (check.value() ? "[PASS] " : "[FAIL] ") << check.key() << '\n';
        if (!result.passed)
            output << "FAILURE CATEGORY: " << result.failureCategory << '\n';
        output << "RESULT: " << (result.passed ? "PASS" : "FAIL") << '\n';
    } else {
        output << "CASE " << testCase.caseId << ": PASS (" << result.elapsedMs << " ms)\n";
    }
    return result.passed ? 0 : 1;
}
