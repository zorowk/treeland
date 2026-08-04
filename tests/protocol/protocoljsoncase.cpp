// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "protocoljsoncase.h"

#include <QCryptographicHash>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QSet>

#include <cmath>

namespace {
const QString WindowManagementInterface =
    QStringLiteral("treeland_window_management_v1");
const QString WineWindowManagementProtocol =
    QStringLiteral("treeland_wine_window_management_v1");
const QString WineWindowManagerInterface =
    QStringLiteral("treeland_wine_window_manager_v1");

bool fail(ProtocolJsonValidationError &error,
          const QString &category,
          const QString &message)
{
    error.category = category;
    error.message = message;
    return false;
}

bool readObject(const QString &path,
                const QString &kind,
                QJsonObject &object,
                ProtocolJsonValidationError &error)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return fail(error,
                    QStringLiteral("schema_error"),
                    QStringLiteral("Cannot open %1 JSON: %2").arg(kind, file.errorString()));
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        return fail(error,
                    QStringLiteral("schema_error"),
                    QStringLiteral("Malformed %1 JSON: %2").arg(kind, parseError.errorString()));
    }
    object = document.object();
    return true;
}

bool isUnsignedInteger(const QJsonValue &value)
{
    if (!value.isDouble())
        return false;
    const double number = value.toDouble();
    return number >= 0 && number <= UINT32_MAX && std::floor(number) == number;
}

const QJsonObject *findInterface(const QJsonObject &metadata,
                                 const QString &name,
                                 QJsonObject &storage)
{
    for (const QJsonValue &value : metadata.value(QStringLiteral("interfaces")).toArray()) {
        const QJsonObject interface = value.toObject();
        if (interface.value(QStringLiteral("name")).toString() == name) {
            storage = interface;
            return &storage;
        }
    }
    return nullptr;
}

bool validateArguments(const QJsonArray &actual,
                       const QJsonArray &declared,
                       const QString &context,
                       ProtocolJsonValidationError &error)
{
    if (actual.size() != declared.size()) {
        return fail(error,
                    QStringLiteral("adapter_validation_error"),
                    QStringLiteral("%1 expects %2 arguments, got %3")
                        .arg(context)
                        .arg(declared.size())
                        .arg(actual.size()));
    }

    for (qsizetype i = 0; i < actual.size(); ++i) {
        const QString type = declared.at(i).toObject().value(QStringLiteral("type")).toString();
        if (type == QStringLiteral("uint") && !isUnsignedInteger(actual.at(i))) {
            return fail(error,
                        QStringLiteral("adapter_validation_error"),
                        QStringLiteral("%1 argument %2 must be uint").arg(context).arg(i));
        }
    }
    return true;
}

bool validateMetadataShape(const QJsonObject &metadata,
                           ProtocolJsonValidationError &error)
{
    if (metadata.value(QStringLiteral("protocol")).toString()
            != WindowManagementInterface
        || !metadata.value(QStringLiteral("interfaces")).isArray()) {
        return fail(error,
                    QStringLiteral("metadata_validation_error"),
                    QStringLiteral("Generated metadata does not describe window management"));
    }
    return true;
}

bool validateInput(const QJsonObject &input,
                   const QJsonObject &interface,
                   ProtocolJsonValidationError &error)
{
    if (input.value(QStringLiteral("schema_version")).toInt(-1) != 1
        || input.value(QStringLiteral("case")).toString().isEmpty()
        || !input.value(QStringLiteral("protocol")).isObject()
        || !input.value(QStringLiteral("steps")).isArray()) {
        return fail(error,
                    QStringLiteral("schema_error"),
                    QStringLiteral("Input must contain schema_version 1, case, protocol, and steps"));
    }

    const QJsonObject protocol = input.value(QStringLiteral("protocol")).toObject();
    if (protocol.value(QStringLiteral("interface")).toString()
            != WindowManagementInterface
        || protocol.value(QStringLiteral("version")).toInt(-1)
            != interface.value(QStringLiteral("version")).toInt(-2)) {
        return fail(error,
                    QStringLiteral("metadata_validation_error"),
                    QStringLiteral("Input interface or version does not match generated metadata"));
    }

    QSet<QString> checkpoints;
    QString boundObject;
    bool sawBind = false;
    bool sawDestroy = false;
    bool sawDisconnect = false;
    const QJsonArray requests = interface.value(QStringLiteral("requests")).toArray();
    for (const QJsonValue &stepValue : input.value(QStringLiteral("steps")).toArray()) {
        if (!stepValue.isObject() || stepValue.toObject().size() != 1) {
            return fail(error,
                        QStringLiteral("schema_error"),
                        QStringLiteral("Each input step must be an object with one operation"));
        }
        const QJsonObject step = stepValue.toObject();
        const QString operation = step.constBegin().key();
        if (operation == QStringLiteral("bind")) {
            const QJsonObject bind = step.value(operation).toObject();
            if (sawBind || bind.value(QStringLiteral("object")).toString().isEmpty()
                || bind.value(QStringLiteral("interface")).toString()
                    != WindowManagementInterface
                || bind.value(QStringLiteral("version")).toInt(-1)
                    != interface.value(QStringLiteral("version")).toInt(-2)) {
                return fail(error,
                            QStringLiteral("metadata_validation_error"),
                            QStringLiteral("Bind step does not match generated interface metadata"));
            }
            sawBind = true;
            boundObject = bind.value(QStringLiteral("object")).toString();
        } else if (operation == QStringLiteral("request")) {
            if (!sawBind || sawDestroy || sawDisconnect || !step.value(operation).isObject()) {
                return fail(error,
                            QStringLiteral("schema_error"),
                            QStringLiteral("Request must occur after bind and before disconnect"));
            }
            const QJsonObject request = step.value(operation).toObject();
            const QString name = request.value(QStringLiteral("name")).toString();
            if (request.value(QStringLiteral("object")).toString() != boundObject
                || !request.value(QStringLiteral("args")).isArray() || name.isEmpty()) {
                return fail(error,
                            QStringLiteral("adapter_validation_error"),
                            QStringLiteral("Request target, name, or args are invalid"));
            }
            QJsonObject declaration;
            for (const QJsonValue &candidate : requests) {
                if (candidate.toObject().value(QStringLiteral("name")).toString() == name) {
                    declaration = candidate.toObject();
                    break;
                }
            }
            if (declaration.isEmpty()) {
                return fail(error,
                            QStringLiteral("metadata_validation_error"),
                            QStringLiteral("Unknown request in generated metadata: %1").arg(name));
            }
            if (!validateArguments(request.value(QStringLiteral("args")).toArray(),
                                   declaration.value(QStringLiteral("arguments")).toArray(),
                                   QStringLiteral("request %1").arg(name),
                                   error)) {
                return false;
            }
            sawDestroy = declaration.value(QStringLiteral("destructor")).toBool();
        } else if (operation == QStringLiteral("client_roundtrip")) {
            if (!step.value(operation).isObject() || !sawBind || sawDisconnect) {
                return fail(error,
                            QStringLiteral("schema_error"),
                            QStringLiteral("client_roundtrip must occur while the client is connected"));
            }
        } else if (operation == QStringLiteral("checkpoint")) {
            const QString name = step.value(operation).toString();
            if (!sawBind || sawDisconnect || name.isEmpty() || checkpoints.contains(name)) {
                return fail(error,
                            QStringLiteral("schema_error"),
                            QStringLiteral("Checkpoint names must be non-empty and unique"));
            }
            checkpoints.insert(name);
        } else if (operation == QStringLiteral("disconnect")) {
            if (!step.value(operation).isObject() || !sawBind || !sawDestroy || sawDisconnect
                || stepValue != input.value(QStringLiteral("steps")).toArray().last()) {
                return fail(error,
                            QStringLiteral("schema_error"),
                            QStringLiteral("disconnect must be the final step"));
            }
            sawDisconnect = true;
        } else {
            return fail(error,
                        QStringLiteral("schema_error"),
                        QStringLiteral("Unsupported input operation: %1").arg(operation));
        }
    }

    if (!sawBind || !sawDestroy || !sawDisconnect || checkpoints.isEmpty()) {
        return fail(error,
                    QStringLiteral("schema_error"),
                    QStringLiteral("Input requires bind, checkpoint, protocol destroy, and disconnect steps"));
    }
    return true;
}

bool validateExpected(const QJsonObject &expected,
                      const QJsonObject &interface,
                      const QString &xmlSha256,
                      ProtocolJsonValidationError &error)
{
    if (expected.value(QStringLiteral("schema_version")).toInt(-1) != 1
        || expected.value(QStringLiteral("case")).toString().isEmpty()
        || !expected.value(QStringLiteral("expectation_source")).isObject()
        || !expected.value(QStringLiteral("checkpoints")).isObject()) {
        return fail(error,
                    QStringLiteral("schema_error"),
                    QStringLiteral("Expected must contain schema_version 1, case, provenance, and checkpoints"));
    }

    const QJsonObject source = expected.value(QStringLiteral("expectation_source")).toObject();
    const QString reviewStatus = source.value(QStringLiteral("review_status")).toString();
    if (source.value(QStringLiteral("generated_by")).toString().isEmpty()
        || !source.value(QStringLiteral("based_on")).isArray()
        || source.value(QStringLiteral("server_commit")).toString().isEmpty()
        || (reviewStatus != QStringLiteral("candidate")
            && reviewStatus != QStringLiteral("human-reviewed"))) {
        return fail(error,
                    QStringLiteral("schema_error"),
                    QStringLiteral("Expected provenance is incomplete"));
    }
    if (source.value(QStringLiteral("xml_sha256")).toString() != xmlSha256) {
        return fail(error,
                    QStringLiteral("metadata_validation_error"),
                    QStringLiteral("Expected XML hash does not match the protocol XML"));
    }

    QJsonObject eventDeclaration;
    for (const QJsonValue &candidate : interface.value(QStringLiteral("events")).toArray()) {
        if (candidate.toObject().value(QStringLiteral("name")).toString()
            == QStringLiteral("show_desktop")) {
            eventDeclaration = candidate.toObject();
            break;
        }
    }
    if (eventDeclaration.isEmpty()) {
        return fail(error,
                    QStringLiteral("metadata_validation_error"),
                    QStringLiteral("Generated metadata lacks show_desktop"));
    }

    const QJsonObject checkpoints = expected.value(QStringLiteral("checkpoints")).toObject();
    if (checkpoints.isEmpty()) {
        return fail(error,
                    QStringLiteral("schema_error"),
                    QStringLiteral("Expected checkpoints cannot be empty"));
    }
    for (auto checkpoint = checkpoints.constBegin();
         checkpoint != checkpoints.constEnd();
         ++checkpoint) {
        if (!checkpoint.value().isObject()) {
            return fail(error,
                        QStringLiteral("schema_error"),
                        QStringLiteral("Checkpoint %1 must be an object").arg(checkpoint.key()));
        }
        const QJsonObject value = checkpoint.value().toObject();
        if (!value.value(QStringLiteral("client_events")).isArray()
            || !value.value(QStringLiteral("server_state")).isObject()) {
            return fail(error,
                        QStringLiteral("schema_error"),
                        QStringLiteral("Checkpoint %1 requires client_events and server_state")
                            .arg(checkpoint.key()));
        }
        for (const QJsonValue &eventValue : value.value(QStringLiteral("client_events")).toArray()) {
            const QJsonObject event = eventValue.toObject();
            if (event.value(QStringLiteral("object")).toString().isEmpty()
                || event.value(QStringLiteral("event")).toString()
                    != QStringLiteral("show_desktop")
                || !event.value(QStringLiteral("args")).isArray()) {
                return fail(error,
                            QStringLiteral("metadata_validation_error"),
                            QStringLiteral("Checkpoint %1 contains an unknown event")
                                .arg(checkpoint.key()));
            }
            if (!validateArguments(event.value(QStringLiteral("args")).toArray(),
                                   eventDeclaration.value(QStringLiteral("arguments")).toArray(),
                                   QStringLiteral("event show_desktop"),
                                   error)) {
                return false;
            }
        }
        const QJsonObject serverState = value.value(QStringLiteral("server_state")).toObject();
        if (!isUnsignedInteger(serverState.value(QStringLiteral("desktop_state")))
            || !isUnsignedInteger(
                serverState.value(QStringLiteral("desktop_state_changed_count")))) {
            return fail(error,
                        QStringLiteral("schema_error"),
                        QStringLiteral("Checkpoint %1 has malformed server_state")
                            .arg(checkpoint.key()));
        }
    }
    return true;
}

bool validateWineCase(const ProtocolJsonCase &testCase,
                      const QJsonObject &managerInterface,
                      ProtocolJsonValidationError &error)
{
    const QJsonObject input = testCase.input;
    const QJsonObject expected = testCase.expected;
    const QString validationMode = input.value(QStringLiteral("validation_mode"))
                                       .toString(QStringLiteral("strict"));
    if (input.value(QStringLiteral("schema_version")).toInt(-1) != 1
        || input.value(QStringLiteral("case")).toString().isEmpty()
        || input.value(QStringLiteral("protocol")).toObject()
               .value(QStringLiteral("interface")).toString() != WineWindowManagerInterface
        || !input.value(QStringLiteral("server")).isObject()
        || !input.value(QStringLiteral("client")).isObject()
        || !input.value(QStringLiteral("steps")).isArray()) {
        return fail(error, QStringLiteral("schema_error"),
                    QStringLiteral("Wine input schema is incomplete"));
    }
    if (validationMode != QStringLiteral("strict")
        && validationMode != QStringLiteral("wire")) {
        return fail(error, QStringLiteral("schema_error"),
                    QStringLiteral("validation_mode must be strict or wire"));
    }
    if (input.value(QStringLiteral("protocol")).toObject()
            .value(QStringLiteral("version")).toInt(-1)
        != managerInterface.value(QStringLiteral("version")).toInt(-2)) {
        return fail(error, QStringLiteral("metadata_validation_error"),
                    QStringLiteral("Wine protocol version differs from generated metadata"));
    }
    const QJsonArray outputs = input.value(QStringLiteral("server")).toObject()
                                   .value(QStringLiteral("outputs")).toArray();
    const QJsonArray objects = input.value(QStringLiteral("client")).toObject()
                                   .value(QStringLiteral("objects")).toArray();
    if (outputs.size() != 1
        || outputs.first().toObject().value(QStringLiteral("geometry")).toArray().size() != 4
        || objects.size() != 2
        || objects.at(0).toObject().value(QStringLiteral("fixture")).toString()
            != QStringLiteral("xdg_toplevel")
        || !objects.at(0).toObject().value(QStringLiteral("mapped")).toBool()
        || objects.at(1).toObject().value(QStringLiteral("interface")).toString()
            != QStringLiteral("treeland_wine_window_control_v1")
        || objects.at(1).toObject().value(QStringLiteral("for")).toString()
            != QStringLiteral("$window")) {
        return fail(error, QStringLiteral("adapter_validation_error"),
                    QStringLiteral("Wine fixture object table is invalid"));
    }

    QSet<QString> captures;
    QSet<QString> checkpoints;
    bool sawRequest = false;
    bool sawServerCondition = false;
    bool sawDestroy = false;
    bool sawDisconnect = false;
    bool expectsProtocolError = false;
    const bool disconnectBeforeCompletion = input.value(QStringLiteral("case")).toString()
        .endsWith(QStringLiteral("disconnect-before-completion"));
    for (const QJsonValue &value : input.value(QStringLiteral("steps")).toArray()) {
        const QJsonObject step = value.toObject();
        if (step.size() != 1)
            return fail(error, QStringLiteral("schema_error"),
                        QStringLiteral("Each Wine step must contain one operation"));
        if (step.contains(QStringLiteral("capture"))) {
            const QJsonObject capture = step.value(QStringLiteral("capture")).toObject();
            const QString name = capture.value(QStringLiteral("name")).toString();
            if (name.isEmpty() || !isUnsignedInteger(capture.value(QStringLiteral("value"))))
                return fail(error, QStringLiteral("schema_error"),
                            QStringLiteral("Serial capture is invalid"));
            captures.insert(name);
        } else if (step.contains(QStringLiteral("request"))) {
            const QJsonObject request = step.value(QStringLiteral("request")).toObject();
            const QJsonArray args = request.value(QStringLiteral("args")).toArray();
            const QString requestName = request.value(QStringLiteral("name")).toString();
            if (request.value(QStringLiteral("object")).toString() != QStringLiteral("control")) {
                return fail(error, QStringLiteral("adapter_validation_error"),
                            QStringLiteral("Wine request target is invalid"));
            }
            if (requestName == QStringLiteral("set_position")) {
                if (args.size() != 3 || !args.at(0).isDouble() || !args.at(1).isDouble()
                    || !args.at(2).isString()
                    || !args.at(2).toString().startsWith(QLatin1Char('$'))
                    || !captures.contains(args.at(2).toString().mid(1))) {
                    return fail(
                        error, QStringLiteral("adapter_validation_error"),
                        QStringLiteral("set_position arguments or serial reference are invalid"));
                }
            } else if (requestName == QStringLiteral("set_z_order")) {
                if (args.size() != 2 || !isUnsignedInteger(args.at(0))
                    || !isUnsignedInteger(args.at(1)) || args.at(0).toInteger() > 4) {
                    return fail(error, QStringLiteral("adapter_validation_error"),
                                QStringLiteral("set_z_order arguments are invalid"));
                }
                const bool invalidSibling = args.at(0).toInteger() != 4
                    && args.at(1).toInteger() != 0;
                if (invalidSibling && validationMode == QStringLiteral("strict")) {
                    return fail(error, QStringLiteral("adapter_validation_error"),
                                QStringLiteral("set_z_order sibling_id must be zero for this op"));
                }
                expectsProtocolError = invalidSibling;
            } else {
                return fail(error, QStringLiteral("metadata_validation_error"),
                            QStringLiteral("Unsupported Wine request: %1").arg(requestName));
            }
            sawRequest = true;
        } else if (step.contains(QStringLiteral("barrier"))) {
            const QJsonObject barrier = step.value(QStringLiteral("barrier")).toObject();
            const QString type = barrier.value(QStringLiteral("type")).toString();
            if (type == QStringLiteral("server_condition")) {
                if (barrier.value(QStringLiteral("probe")).toString()
                        != QStringLiteral("surface.geometry")
                    || barrier.value(QStringLiteral("selector")).toObject()
                           .value(QStringLiteral("app_id")).toString().isEmpty()
                    || barrier.value(QStringLiteral("equals")).toArray().size() != 4) {
                    return fail(error, QStringLiteral("schema_error"),
                                QStringLiteral("server_condition is invalid"));
                }
                sawServerCondition = true;
            } else if (type != QStringLiteral("client_roundtrip")) {
                return fail(error, QStringLiteral("schema_error"),
                            QStringLiteral("Unknown Wine barrier type"));
            }
        } else if (step.contains(QStringLiteral("checkpoint"))) {
            checkpoints.insert(step.value(QStringLiteral("checkpoint")).toString());
        } else if (step.contains(QStringLiteral("destroy"))) {
            sawDestroy = true;
        } else if (step.contains(QStringLiteral("disconnect"))) {
            sawDisconnect = true;
        } else {
            return fail(error, QStringLiteral("schema_error"),
                        QStringLiteral("Unsupported Wine operation"));
        }
    }
    if (!sawRequest || !sawDisconnect || (checkpoints.isEmpty() && expectsProtocolError)
        || (!disconnectBeforeCompletion && !expectsProtocolError
            && (!sawServerCondition || !sawDestroy || checkpoints.isEmpty()))) {
        return fail(error, QStringLiteral("schema_error"),
                    QStringLiteral("Wine case lacks request, barrier, checkpoint, destroy, or disconnect"));
    }

    const QJsonObject source = expected.value(QStringLiteral("expectation_source")).toObject();
    const QString reviewStatus = source.value(QStringLiteral("review_status")).toString();
    if (expected.value(QStringLiteral("schema_version")).toInt(-1) != 1
        || (reviewStatus != QStringLiteral("candidate")
            && reviewStatus != QStringLiteral("human-reviewed"))
        || source.value(QStringLiteral("xml_sha256")).toString() != testCase.xmlSha256
        || source.value(QStringLiteral("server_commit")).toString().isEmpty()) {
        return fail(error, QStringLiteral("metadata_validation_error"),
                    QStringLiteral("Wine expected provenance is incomplete or does not match"));
    }
    const QJsonObject expectedCheckpoints = expected.value(QStringLiteral("checkpoints")).toObject();
    for (const QString &checkpoint : checkpoints) {
        const QJsonObject value = expectedCheckpoints.value(checkpoint).toObject();
        const QJsonObject connection = value.value(QStringLiteral("connection")).toObject();
        if (!value.value(QStringLiteral("client_events")).isArray()
            || !value.value(QStringLiteral("server_state")).isObject()
            || !value.value(QStringLiteral("connection")).isObject()
            || !connection.value(QStringLiteral("display_error")).isDouble()
            || !connection.value(QStringLiteral("protocol_error_occurred")).isBool()) {
            return fail(error, QStringLiteral("schema_error"),
                        QStringLiteral("Wine checkpoint is incomplete"));
        }
        if (connection.value(QStringLiteral("protocol_error_occurred")).toBool()) {
            const QJsonObject protocolError =
                connection.value(QStringLiteral("protocol_error")).toObject();
            if (protocolError.value(QStringLiteral("interface")).toString().isEmpty()
                || !isUnsignedInteger(protocolError.value(QStringLiteral("code")))
                || protocolError.value(QStringLiteral("object")).toString().isEmpty()) {
                return fail(error, QStringLiteral("schema_error"),
                            QStringLiteral("Expected protocol error is incomplete"));
            }
        }
    }
    return expectedCheckpoints.size() == checkpoints.size()
        || fail(error, QStringLiteral("schema_error"),
                QStringLiteral("Wine checkpoint sets differ"));
}
}

QString protocolJsonInputInterface(const QString &inputPath)
{
    QFile file(inputPath);
    if (!file.open(QIODevice::ReadOnly))
        return {};
    return QJsonDocument::fromJson(file.readAll()).object()
        .value(QStringLiteral("protocol")).toObject()
        .value(QStringLiteral("interface")).toString();
}

bool loadProtocolJsonCase(const QString &inputPath,
                          const QString &expectedPath,
                          const QString &metadataPath,
                          const QString &xmlPath,
                          ProtocolJsonCase &testCase,
                          ProtocolJsonValidationError &error)
{
    error = {};
    if (!readObject(inputPath, QStringLiteral("input"), testCase.input, error)
        || !readObject(expectedPath, QStringLiteral("expected"), testCase.expected, error)
        || !readObject(metadataPath, QStringLiteral("metadata"), testCase.metadata, error)) {
        return false;
    }

    QFile xml(xmlPath);
    if (!xml.open(QIODevice::ReadOnly)) {
        return fail(error,
                    QStringLiteral("metadata_validation_error"),
                    QStringLiteral("Cannot open protocol XML: %1").arg(xml.errorString()));
    }
    testCase.xmlSha256 = QString::fromLatin1(
        QCryptographicHash::hash(xml.readAll(), QCryptographicHash::Sha256).toHex());

    const bool wine = testCase.metadata.value(QStringLiteral("protocol")).toString()
        == WineWindowManagementProtocol;
    if (!wine && !validateMetadataShape(testCase.metadata, error))
        return false;
    QJsonObject interfaceStorage;
    const QJsonObject *interface = findInterface(
        testCase.metadata,
        wine ? WineWindowManagerInterface : WindowManagementInterface,
        interfaceStorage);
    if (!interface)
        return fail(error,
                    QStringLiteral("metadata_validation_error"),
                    QStringLiteral("Generated metadata lacks the requested interface"));
    if ((wine && !validateWineCase(testCase, *interface, error))
        || (!wine && (!validateInput(testCase.input, *interface, error)
                      || !validateExpected(testCase.expected,
                                           *interface,
                                           testCase.xmlSha256,
                                           error)))) {
        return false;
    }

    testCase.caseId = testCase.input.value(QStringLiteral("case")).toString();
    if (testCase.caseId != testCase.expected.value(QStringLiteral("case")).toString()) {
        return fail(error,
                    QStringLiteral("schema_error"),
                    QStringLiteral("Input and expected case ID mismatch"));
    }

    if (wine)
        return true;

    const QJsonObject expectedCheckpoints =
        testCase.expected.value(QStringLiteral("checkpoints")).toObject();
    QSet<QString> inputCheckpoints;
    QString boundObject;
    for (const QJsonValue &stepValue : testCase.input.value(QStringLiteral("steps")).toArray()) {
        const QJsonObject step = stepValue.toObject();
        if (step.contains(QStringLiteral("bind"))) {
            boundObject = step.value(QStringLiteral("bind"))
                              .toObject()
                              .value(QStringLiteral("object"))
                              .toString();
        }
        if (step.contains(QStringLiteral("checkpoint"))) {
            const QString checkpoint = step.value(QStringLiteral("checkpoint")).toString();
            inputCheckpoints.insert(checkpoint);
            if (!expectedCheckpoints.contains(checkpoint)) {
                return fail(error,
                            QStringLiteral("schema_error"),
                            QStringLiteral("Expected is missing checkpoint %1").arg(checkpoint));
            }
        }
    }
    if (inputCheckpoints.size() != expectedCheckpoints.size()) {
        return fail(error,
                    QStringLiteral("schema_error"),
                    QStringLiteral("Input and expected checkpoint sets differ"));
    }
    for (const QJsonValue &checkpointValue : expectedCheckpoints) {
        for (const QJsonValue &eventValue :
             checkpointValue.toObject().value(QStringLiteral("client_events")).toArray()) {
            if (eventValue.toObject().value(QStringLiteral("object")).toString() != boundObject) {
                return fail(error,
                            QStringLiteral("adapter_validation_error"),
                            QStringLiteral("Expected event references an unknown object"));
            }
        }
    }
    return true;
}
