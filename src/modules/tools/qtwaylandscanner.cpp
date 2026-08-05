// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include <QCoreApplication>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QXmlStreamReader>

#include <utility>
#include <vector>

class Scanner
{
public:
    explicit Scanner() { }

    ~Scanner()
    {
        delete m_xml;
    }

    bool parseArguments(int argc, char **argv);
    void printUsage();
    bool process();
    void printErrors();

private:
    struct WaylandEnumEntry
    {
        QByteArray name;
        QByteArray value;
        QByteArray summary;
    };

    struct WaylandEnum
    {
        QByteArray name;

        std::vector<WaylandEnumEntry> entries;
    };

    struct WaylandArgument
    {
        QByteArray name;
        QByteArray type;
        QByteArray interface;
        QByteArray summary;
        bool allowNull;
    };

    struct WaylandEvent
    {
        bool request;
        QByteArray name;
        QByteArray type;
        int since = 1;
        std::vector<WaylandArgument> arguments;
    };

    struct WaylandInterface
    {
        QByteArray name;
        int version;

        std::vector<WaylandEnum> enums;
        std::vector<WaylandEvent> events;
        std::vector<WaylandEvent> requests;
    };

    bool isServerSide();
    bool isTestClient();
    bool parseOption(const QByteArray &str);

    QByteArray byteArrayValue(const QXmlStreamReader &xml, const char *name);
    int intValue(const QXmlStreamReader &xml, const char *name, int defaultValue = 0);
    bool boolValue(const QXmlStreamReader &xml, const char *name);
    WaylandEvent readEvent(QXmlStreamReader &xml, bool request);
    Scanner::WaylandEnum readEnum(QXmlStreamReader &xml);
    Scanner::WaylandInterface readInterface(QXmlStreamReader &xml);
    QByteArray waylandToCType(const QByteArray &waylandType, const QByteArray &interface);
    QByteArray waylandToQtType(const QByteArray &waylandType,
                               const QByteArray &interface,
                               bool cStyleArray);
    const Scanner::WaylandArgument *newIdArgument(const std::vector<WaylandArgument> &arguments);

    void printEvent(const WaylandEvent &e, bool omitNames = false, bool withResource = false);
    void printEventHandlerSignature(const WaylandEvent &e,
                                    const char *interfaceName,
                                    bool deepIndent = true);
    void printEnums(const std::vector<WaylandEnum> &enums);
    void printTestClientHeader(const std::vector<WaylandInterface> &interfaces);
    void printTestClientCode(const std::vector<WaylandInterface> &interfaces);
    void printTestClientMetadata(const std::vector<WaylandInterface> &interfaces);

    QByteArray stripInterfaceName(const QByteArray &name);
    QByteArray testAdapterName(const QByteArray &interfaceName) const;
    QByteArray testClientCType(const WaylandArgument &argument) const;
    bool ignoreInterface(const QByteArray &name);
    std::vector<const WaylandInterface *>
    findChildInterfaces(const std::vector<WaylandInterface> &interfaces,
                        const WaylandInterface &parent) const;
    static QByteArray childAdapterPrefix(const QByteArray &interfaceName);
    bool needsPerEventStructs(const WaylandInterface &iface) const;
    static QByteArray perEventCFieldType(const WaylandArgument &argument);
    static bool perEventFieldNeedsCleanup(const WaylandArgument &argument);
    static const WaylandInterface *
    selectTargetInterface(const std::vector<WaylandInterface> &interfaces);

    enum Option
    {
        ClientHeader,
        ServerHeader,
        ClientCode,
        ServerCode,
        TestClientHeader,
        TestClientCode,
        TestClientMetadata,
    } m_option;

    QByteArray m_protocolName;
    QByteArray m_protocolFilePath;
    QByteArray m_scannerName;
    QByteArray m_headerPath;
    QByteArray m_prefix;
    QList<QByteArray> m_includes;
    QXmlStreamReader *m_xml = nullptr;
};

bool Scanner::parseArguments(int argc, char **argv)
{
    QList<QByteArray> args;
    args.reserve(argc);
    for (int i = 0; i < argc; ++i)
        args << QByteArray(argv[i]);

    m_scannerName = args[0];

    if (argc <= 2 || !parseOption(args[1]))
        return false;

    m_protocolFilePath = args[2];

    if (argc > 3 && !args[3].startsWith('-')) {
        // legacy positional arguments
        m_headerPath = args[3];
        if (argc == 5)
            m_prefix = args[4];
    } else {
        // --header-path=<path> (14 characters)
        // --prefix=<prefix> (9 characters)
        // --add-include=<include> (14 characters)
        for (int pos = 3; pos < argc; pos++) {
            const QByteArray &option = args[pos];
            if (option.startsWith("--header-path=")) {
                m_headerPath = option.mid(14);
            } else if (option.startsWith("--prefix=")) {
                m_prefix = option.mid(10);
            } else if (option.startsWith("--add-include=")) {
                auto include = option.mid(14);
                if (!include.isEmpty())
                    m_includes << include;
            } else {
                return false;
            }
        }
    }

    return true;
}

void Scanner::printUsage()
{
    fprintf(stderr,
            "Usage: %s [client-header|server-header|client-code|server-code|test-client-header|"
            "test-client-code|test-client-metadata] specfile "
            "[--header-path=<path>] [--prefix=<prefix>] [--add-include=<include>]\n",
            m_scannerName.constData());
}

bool Scanner::isServerSide()
{
    return m_option == ServerHeader || m_option == ServerCode;
}

bool Scanner::isTestClient()
{
    return m_option == TestClientHeader || m_option == TestClientCode
        || m_option == TestClientMetadata;
}

bool Scanner::parseOption(const QByteArray &str)
{
    if (str == "client-header")
        m_option = ClientHeader;
    else if (str == "server-header")
        m_option = ServerHeader;
    else if (str == "client-code")
        m_option = ClientCode;
    else if (str == "server-code")
        m_option = ServerCode;
    else if (str == "test-client-header")
        m_option = TestClientHeader;
    else if (str == "test-client-code")
        m_option = TestClientCode;
    else if (str == "test-client-metadata")
        m_option = TestClientMetadata;
    else
        return false;

    return true;
}

QByteArray Scanner::byteArrayValue(const QXmlStreamReader &xml, const char *name)
{
    if (xml.attributes().hasAttribute(name))
        return xml.attributes().value(name).toUtf8();
    return QByteArray();
}

int Scanner::intValue(const QXmlStreamReader &xml, const char *name, int defaultValue)
{
    bool ok;
    int result = byteArrayValue(xml, name).toInt(&ok);
    return ok ? result : defaultValue;
}

bool Scanner::boolValue(const QXmlStreamReader &xml, const char *name)
{
    return byteArrayValue(xml, name) == "true";
}

Scanner::WaylandEvent Scanner::readEvent(QXmlStreamReader &xml, bool request)
{
    WaylandEvent event = {
        .request = request,
        .name = byteArrayValue(xml, "name"),
        .type = byteArrayValue(xml, "type"),
        .since = intValue(xml, "since", 1),
        .arguments = {},
    };
    while (xml.readNextStartElement()) {
        if (xml.name() == QLatin1String("arg")) {
            WaylandArgument argument = {
                .name = byteArrayValue(xml, "name"),
                .type = byteArrayValue(xml, "type"),
                .interface = byteArrayValue(xml, "interface"),
                .summary = byteArrayValue(xml, "summary"),
                .allowNull = boolValue(xml, "allow-null"),
            };
            event.arguments.push_back(std::move(argument));
        }

        xml.skipCurrentElement();
    }
    return event;
}

Scanner::WaylandEnum Scanner::readEnum(QXmlStreamReader &xml)
{
    WaylandEnum result = {
        .name = byteArrayValue(xml, "name"),
        .entries = {},
    };

    while (xml.readNextStartElement()) {
        if (xml.name() == QLatin1String("entry")) {
            WaylandEnumEntry entry = {
                .name = byteArrayValue(xml, "name"),
                .value = byteArrayValue(xml, "value"),
                .summary = byteArrayValue(xml, "summary"),
            };
            result.entries.push_back(std::move(entry));
        }

        xml.skipCurrentElement();
    }

    return result;
}

Scanner::WaylandInterface Scanner::readInterface(QXmlStreamReader &xml)
{
    WaylandInterface interface = {
        .name = byteArrayValue(xml, "name"),
        .version = intValue(xml, "version", 1),
        .enums = {},
        .events = {},
        .requests = {},
    };

    while (xml.readNextStartElement()) {
        if (xml.name() == QLatin1String("event"))
            interface.events.push_back(readEvent(xml, false));
        else if (xml.name() == QLatin1String("request"))
            interface.requests.push_back(readEvent(xml, true));
        else if (xml.name() == QLatin1String("enum"))
            interface.enums.push_back(readEnum(xml));
        else
            xml.skipCurrentElement();
    }

    return interface;
}

QByteArray Scanner::waylandToCType(const QByteArray &waylandType, const QByteArray &interface)
{
    if (waylandType == "string")
        return "const char *";
    else if (waylandType == "int")
        return "int32_t";
    else if (waylandType == "uint")
        return "uint32_t";
    else if (waylandType == "fixed")
        return "wl_fixed_t";
    else if (waylandType == "fd")
        return "int32_t";
    else if (waylandType == "array")
        return "wl_array *";
    else if (waylandType == "object" || waylandType == "new_id") {
        if (isServerSide())
            return "struct ::wl_resource *";
        if (interface.isEmpty())
            return "struct ::wl_object *";
        return "struct ::" + interface + " *";
    }
    return waylandType;
}

QByteArray Scanner::waylandToQtType(const QByteArray &waylandType,
                                    const QByteArray &interface,
                                    bool cStyleArray)
{
    if (waylandType == "string")
        return "const QString &";
    else if (waylandType == "array")
        return cStyleArray ? "wl_array *" : "const QByteArray &";
    else
        return waylandToCType(waylandType, interface);
}

const Scanner::WaylandArgument *Scanner::newIdArgument(
    const std::vector<WaylandArgument> &arguments)
{
    for (const WaylandArgument &a : arguments) {
        if (a.type == "new_id")
            return &a;
    }
    return nullptr;
}

void Scanner::printEvent(const WaylandEvent &e, bool omitNames, bool withResource)
{
    printf("%s(", e.name.constData());
    bool needsComma = false;
    if (isServerSide()) {
        if (e.request) {
            printf("Resource *%s", omitNames ? "" : "resource");
            needsComma = true;
        } else if (withResource) {
            printf("struct ::wl_resource *%s", omitNames ? "" : "resource");
            needsComma = true;
        }
    }
    for (const WaylandArgument &a : e.arguments) {
        bool isNewId = a.type == "new_id";
        if (isNewId && !isServerSide() && (a.interface.isEmpty() != e.request))
            continue;
        if (needsComma)
            printf(", ");
        needsComma = true;
        if (isNewId) {
            if (isServerSide()) {
                if (e.request) {
                    printf("uint32_t");
                    if (!omitNames)
                        printf(" %s", a.name.constData());
                    continue;
                }
            } else {
                if (e.request) {
                    printf("const struct ::wl_interface *%s, uint32_t%s",
                           omitNames ? "" : "interface",
                           omitNames ? "" : " version");
                    continue;
                }
            }
        }

        QByteArray qtType = waylandToQtType(a.type, a.interface, e.request == isServerSide());
        printf("%s%s%s",
               qtType.constData(),
               qtType.endsWith("&") || qtType.endsWith("*") ? "" : " ",
               omitNames ? "" : a.name.constData());
    }
    printf(")");
}

void Scanner::printEventHandlerSignature(const WaylandEvent &e,
                                         const char *interfaceName,
                                         bool deepIndent)
{
    const char *indent = deepIndent ? "    " : "";
    printf("handle_%s(\n", e.name.constData());
    if (isServerSide()) {
        printf("        %s::wl_client *client,\n", indent);
        printf("        %sstruct wl_resource *resource", indent);
    } else {
        printf("        %svoid *data,\n", indent);
        printf("        %sstruct ::%s *object", indent, interfaceName);
    }
    for (const WaylandArgument &a : e.arguments) {
        printf(",\n");
        bool isNewId = a.type == "new_id";
        if (isServerSide() && isNewId) {
            printf("        %suint32_t %s", indent, a.name.constData());
        } else {
            QByteArray cType = waylandToCType(a.type, a.interface);
            printf("        %s%s%s%s",
                   indent,
                   cType.constData(),
                   cType.endsWith("*") ? "" : " ",
                   a.name.constData());
        }
    }
    printf(")");
}

void Scanner::printEnums(const std::vector<WaylandEnum> &enums)
{
    for (const WaylandEnum &e : enums) {
        printf("\n");
        printf("        enum %s {\n", e.name.constData());
        for (const WaylandEnumEntry &entry : e.entries) {
            printf("            %s_%s = %s,",
                   e.name.constData(),
                   entry.name.constData(),
                   entry.value.constData());
            if (!entry.summary.isNull())
                printf(" // %s", entry.summary.constData());
            printf("\n");
        }
        printf("        };\n");
    }
}

QByteArray Scanner::stripInterfaceName(const QByteArray &name)
{
    if (!m_prefix.isEmpty() && name.startsWith(m_prefix))
        return name.mid(m_prefix.size());
    if (name.startsWith("qt_") || name.startsWith("wl_"))
        return name.mid(3);

    return name;
}

bool Scanner::ignoreInterface(const QByteArray &name)
{
    return name == "wl_display" || ((isServerSide() || isTestClient()) && name == "wl_registry");
}

QByteArray Scanner::testAdapterName(const QByteArray &interfaceName) const
{
    QByteArray name = interfaceName;
    if (name.startsWith("treeland_"))
        name.remove(0, 9);
    const qsizetype versionMarker = name.lastIndexOf("_v");
    if (versionMarker >= 0) {
        bool isVersion = true;
        for (qsizetype i = versionMarker + 2; i < name.size(); ++i)
            isVersion = isVersion && name.at(i) >= '0' && name.at(i) <= '9';
        if (isVersion)
            name.truncate(versionMarker);
    }
    return "tl_" + name;
}

QByteArray Scanner::testClientCType(const WaylandArgument &argument) const
{
    if (argument.type == "string")
        return "const char *";
    if (argument.type == "int" || argument.type == "fd")
        return "int32_t";
    if (argument.type == "uint")
        return "uint32_t";
    if (argument.type == "fixed")
        return "wl_fixed_t";
    if (argument.type == "array")
        return "struct wl_array *";
    if (argument.type == "object" || argument.type == "new_id") {
        if (argument.interface.isEmpty())
            return "struct wl_proxy *";
        return "struct " + argument.interface + " *";
    }
    return argument.type;
}


std::vector<const Scanner::WaylandInterface *>
Scanner::findChildInterfaces(const std::vector<WaylandInterface> &interfaces,
                             const WaylandInterface &parent) const
{
    std::vector<const WaylandInterface *> children;
    for (const WaylandEvent &event : parent.events) {
        for (const WaylandArgument &arg : event.arguments) {
            if (arg.type != "new_id" || arg.interface.isEmpty())
                continue;
            for (const WaylandInterface &iface : interfaces) {
                if (iface.name == arg.interface) {
                    children.push_back(&iface);
                    break;
                }
            }
        }
    }
    return children;
}

QByteArray Scanner::childAdapterPrefix(const QByteArray &interfaceName)
{
    QByteArray name = interfaceName;
    if (name.startsWith("treeland_"))
        name = name.mid(9);
    int pos = name.lastIndexOf("_v");
    if (pos > 0)
        name = name.left(pos);
    return name;
}

bool Scanner::needsPerEventStructs(const WaylandInterface &iface) const
{
    for (const WaylandEvent &event : iface.events) {
        if (event.arguments.size() > 1)
            return true;
        for (const WaylandArgument &arg : event.arguments) {
            if (arg.type == "int" || arg.type == "string")
                return true;
        }
    }
    return false;
}

QByteArray Scanner::perEventCFieldType(const WaylandArgument &argument)
{
    if (argument.type == "uint" || argument.type == "enum")
        return "uint32_t";
    if (argument.type == "int")
        return "int32_t";
    if (argument.type == "string")
        return "char *";
    if (argument.type == "fixed")
        return "wl_fixed_t";
    if (argument.type == "fd")
        return "int";
    if (argument.type == "object" || argument.type == "new_id") {
        if (argument.interface.isEmpty())
            return "struct wl_proxy *";
        return "struct " + argument.interface + " *";
    }
    return argument.type;
}

bool Scanner::perEventFieldNeedsCleanup(const WaylandArgument &argument)
{
    return argument.type == "string";
}
// Select the best target interface: prefer the one with events.
const Scanner::WaylandInterface *
Scanner::selectTargetInterface(const std::vector<WaylandInterface> &interfaces)
{
    const WaylandInterface *first = nullptr;
    for (const WaylandInterface &iface : interfaces) {
        if (iface.name == "wl_display" || iface.name == "wl_registry")
            continue;
        if (!first)
            first = &iface;
        if (!iface.events.empty())
            return &iface;
    }
    return first;
}

void Scanner::printTestClientHeader(const std::vector<WaylandInterface> &interfaces)
{
    const WaylandInterface *target = selectTargetInterface(interfaces);
    if (!target)
        return;

    const auto children = findChildInterfaces(interfaces, *target);
    const QByteArray adapter = testAdapterName(target->name);
    const QByteArray guard = (adapter + "_TEST_ADAPTER_H").toUpper();
    printf("#ifndef %s\n#define %s\n\n", guard.constData(), guard.constData());
    printf("#include <stdbool.h>\n#include <stddef.h>\n#include <stdint.h>\n");
    printf("#include <wayland-util.h>\n\n");
    printf("struct wl_registry;\nstruct wl_registry_listener;\n");
    printf("struct wl_buffer;\nstruct wl_callback;\nstruct wl_output;\n");
    printf("struct wl_seat;\nstruct wl_surface;\n");
    for (const WaylandInterface &iface : interfaces) {
        if (!ignoreInterface(iface.name))
            printf("struct %s;\n", iface.name.constData());
    }
    printf("\n");
    printf("#ifdef __cplusplus\nextern \"C\" {\n#endif\n\n");
    const QByteArray maxEvents = (adapter + "_MAX_EVENTS").toUpper();
    printf("enum { %s = 8 };\n\n", maxEvents.constData());
    printf("struct %s_fixed_event {\n", adapter.constData());
    printf("    const char *name;\n    wl_fixed_t raw;\n};\n\n");
    printf("struct %s_array_event {\n", adapter.constData());
    printf("    const char *name;\n    uint8_t *data;\n    size_t size;\n};\n\n");
    printf("struct %s_fd_event {\n", adapter.constData());
    printf("    const char *name;\n    int fd;\n};\n\n");
    if (needsPerEventStructs(*target)) {
        for (const WaylandEvent &event : target->events) {
            printf("struct %s_%s_event {\n", adapter.constData(), event.name.constData());
            for (const WaylandArgument &argument : event.arguments) {
                QByteArray ctype = perEventCFieldType(argument);
                printf("    %s %s;\n", ctype.constData(), argument.name.constData());
            }
            printf("};\n\n");
        }
    }
    printf("struct %s_adapter {\n", adapter.constData());
    printf("    struct %s *proxy;\n", target->name.constData());
    printf("    uint32_t global_name;\n    uint32_t advertised_version;\n");
    printf("    uint32_t bound_version;\n");
    printf("    uint32_t events[%s];\n", maxEvents.constData());
    printf("    size_t event_count;\n    bool local_proxy_alive;\n");
    printf("    struct %s_fixed_event fixed_events[%s];\n",
           adapter.constData(), maxEvents.constData());
    printf("    size_t fixed_event_count;\n");
    printf("    struct %s_array_event array_events[%s];\n",
           adapter.constData(), maxEvents.constData());
    printf("    size_t array_event_count;\n");
    printf("    struct %s_fd_event fd_events[%s];\n",
           adapter.constData(), maxEvents.constData());
    printf("    size_t fd_event_count;\n    bool event_snapshot_failed;\n");
    for (const WaylandInterface *child : children) {
        const QByteArray prefix = childAdapterPrefix(child->name);
        printf("    struct %s *%s_proxy;\n", child->name.constData(), prefix.constData());
        printf("    bool %s_listener_installed;\n", prefix.constData());
        printf("    uint32_t %s_events[%s];\n", prefix.constData(), maxEvents.constData());
        printf("    size_t %s_event_count;\n", prefix.constData());
    }
    if (needsPerEventStructs(*target)) {
        for (const WaylandEvent &event : target->events) {
            printf("    struct %s_%s_event %s_events[%s];\n",
                   adapter.constData(), event.name.constData(),
                   event.name.constData(), maxEvents.constData());
            printf("    size_t %s_event_count;\n", event.name.constData());
        }
    }
    printf("    bool protocol_destructor_sent;\n};\n\n");
    printf("void %s_adapter_init(struct %s_adapter *adapter);\n", adapter.constData(), adapter.constData());
    printf("void %s_adapter_fini(struct %s_adapter *adapter);\n", adapter.constData(), adapter.constData());
    printf("const struct wl_registry_listener *%s_registry_listener(void);\n", adapter.constData());
    printf("int %s_bind(struct %s_adapter *adapter, struct wl_registry *registry, "
           "uint32_t requested_version);\n", adapter.constData(), adapter.constData());
    printf("void %s_clear_events(struct %s_adapter *adapter);\n", adapter.constData(), adapter.constData());
    printf("int %s_dispatch(void *adapter_ptr, const char *name, const char **args, int arg_count);\n",
           adapter.constData());
    printf("struct %s_registry_type {\n", adapter.constData());
    printf("    const char *protocol_name;\n    size_t adapter_size;\n");
    printf("    void (*init)(void *);\n    void (*fini)(void *);\n");
    printf("    int (*bind)(void *, struct wl_registry *, uint32_t);\n");
    printf("    void (*clear_events)(void *);\n");
    printf("    int (*dispatch)(void *, const char *, const char **, int);\n");
    printf("    int (*destroy)(void *);\n");
    printf("    const struct wl_registry_listener *(*listener)(void);\n");
    printf("};\n");
    printf("extern const struct %s_registry_type %s_registry;\n", adapter.constData(), adapter.constData());
    for (const WaylandEvent &request : target->requests) {
        printf("int %s_%s(struct %s_adapter *adapter", adapter.constData(), request.name.constData(), adapter.constData());
        for (const WaylandArgument &argument : request.arguments) {
            if (argument.type == "new_id")
                continue;
            const QByteArray type = testClientCType(argument);
            printf(", %s%s%s", type.constData(), type.endsWith('*') ? "" : " ", argument.name.constData());
        }
        printf(");\n");
    }
    printf("\n#ifdef __cplusplus\n}\n#endif\n\n#endif\n");
}

void Scanner::printTestClientCode(const std::vector<WaylandInterface> &interfaces)
{
    const WaylandInterface *target = selectTargetInterface(interfaces);
    if (!target)
        return;

    const QByteArray adapter = testAdapterName(target->name);
    const QByteArray maxEvents = (adapter + "_MAX_EVENTS").toUpper();
    const QByteArray basename = QByteArray(m_protocolName).replace('_', '-');
    printf("#include \"tl-test-%s.h\"\n", basename.constData());
    printf("#include \"wayland-%s-client-protocol.h\"\n\n", basename.constData());
    printf("#include <stdlib.h>\n#include <string.h>\n#include <unistd.h>\n#include <fcntl.h>\n#include <wayland-client.h>\n\n");

    const auto children = findChildInterfaces(interfaces, *target);

    // Generate child event handlers and listener structs.
    for (const WaylandInterface *child : children) {
        const QByteArray prefix = childAdapterPrefix(child->name);
        for (const WaylandEvent &event : child->events) {
            printf("static void handle_%s_%s(void *data, struct %s *proxy",
                   prefix.constData(), event.name.constData(), child->name.constData());
            for (const WaylandArgument &argument : event.arguments) {
                const QByteArray type = testClientCType(argument);
                printf(", %s%s%s", type.constData(), type.endsWith('*') ? "" : " ", argument.name.constData());
            }
            printf(")\n{\n    struct %s_adapter *adapter = data;\n    (void)proxy;\n", adapter.constData());
            if (event.arguments.size() == 1 && event.arguments.front().type == "uint")
                printf("    if (adapter->%s_event_count < %s)\n"
                       "        adapter->%s_events[adapter->%s_event_count++] = %s;\n",
                       prefix.constData(), maxEvents.constData(),
                       prefix.constData(), prefix.constData(),
                       event.arguments.front().name.constData());
            else
                for (const WaylandArgument &argument : event.arguments)
                    printf("    (void)%s;\n", argument.name.constData());
            printf("}\n\n");
        }
        printf("static const struct %s_listener %s_test_listener = {\n",
               child->name.constData(), prefix.constData());
        for (const WaylandEvent &event : child->events)
            printf("    .%s = handle_%s_%s,\n",
                   event.name.constData(), prefix.constData(), event.name.constData());
        printf("};\n\n");
    }

    for (const WaylandEvent &event : target->events) {
        printf("static void handle_%s(void *data, struct %s *proxy",
               event.name.constData(), target->name.constData());
        for (const WaylandArgument &argument : event.arguments) {
            const QByteArray type = testClientCType(argument);
            printf(", %s%s%s", type.constData(), type.endsWith('*') ? "" : " ", argument.name.constData());
        }
        printf(")\n{\n    struct %s_adapter *adapter = data;\n    (void)proxy;\n", adapter.constData());
        if (event.arguments.size() == 1 && event.arguments.front().type == "uint")
            printf("    if (adapter->event_count < %s)\n"
                   "        adapter->events[adapter->event_count++] = %s;\n",
                   maxEvents.constData(),
                   event.arguments.front().name.constData());
        else if (event.arguments.size() == 1 && event.arguments.front().type == "fixed")
            printf("    if (adapter->fixed_event_count < %s) {\n"
                   "        struct %s_fixed_event *event = "
                   "&adapter->fixed_events[adapter->fixed_event_count++];\n"
                   "        event->name = \"%s\";\n"
                   "        event->raw = %s;\n"
                   "    }\n",
                   maxEvents.constData(), adapter.constData(), event.name.constData(),
                   event.arguments.front().name.constData());
        else if (event.arguments.size() == 1 && event.arguments.front().type == "array")
            printf("    if (!%s || (%s->size > 0 && !%s->data) || "
                   "adapter->array_event_count >= %s) {\n"
                   "        adapter->event_snapshot_failed = true;\n"
                   "        return;\n"
                   "    }\n"
                   "    struct %s_array_event *event = "
                   "&adapter->array_events[adapter->array_event_count];\n"
                   "    if (%s->size > 0) {\n"
                   "        event->data = malloc(%s->size);\n"
                   "        if (!event->data) {\n"
                   "            adapter->event_snapshot_failed = true;\n"
                   "            return;\n"
                   "        }\n"
                   "        memcpy(event->data, %s->data, %s->size);\n"
                   "    }\n"
                   "    event->name = \"%s\";\n"
                   "    event->size = %s->size;\n"
                   "    adapter->array_event_count++;\n",
                   event.arguments.front().name.constData(),
                   event.arguments.front().name.constData(),
                   event.arguments.front().name.constData(), maxEvents.constData(),
                   adapter.constData(), event.arguments.front().name.constData(),
                   event.arguments.front().name.constData(),
                   event.arguments.front().name.constData(),
                   event.arguments.front().name.constData(), event.name.constData(),
                   event.arguments.front().name.constData());
        else if (event.arguments.size() == 1 && event.arguments.front().type == "fd")
            printf("    if (%s < 0 || adapter->fd_event_count >= %s) {\n"
                   "        adapter->event_snapshot_failed = true;\n"
                   "        return;\n"
                   "    }\n"
                   "    int owned = fcntl(%s, F_DUPFD_CLOEXEC, 0);\n"
                   "    if (owned < 0) {\n"
                   "        adapter->event_snapshot_failed = true;\n"
                   "        return;\n"
                   "    }\n"
                   "    struct %s_fd_event *event = "
                   "&adapter->fd_events[adapter->fd_event_count];\n"
                   "    event->name = \"%s\";\n"
                   "    event->fd = owned;\n"
                   "    adapter->fd_event_count++;\n",
                   event.arguments.front().name.constData(), maxEvents.constData(),
                   event.arguments.front().name.constData(),
                   adapter.constData(), event.name.constData());
        else if (event.arguments.size() == 1 && event.arguments.front().type == "new_id"
                 && !event.arguments.front().interface.isEmpty()) {
            // Find the child interface matching this new_id argument.
            const auto *childArg = &event.arguments.front();
            const WaylandInterface *childIface = nullptr;
            QByteArray childPrefix;
            for (const WaylandInterface *c : children) {
                if (c->name == childArg->interface) {
                    childIface = c;
                    childPrefix = childAdapterPrefix(c->name);
                    break;
                }
            }
            if (childIface) {
                printf("    if (!%s || adapter->%s_proxy) {\n"
                       "        adapter->event_snapshot_failed = true;\n"
                       "        return;\n"
                       "    }\n"
                       "    adapter->%s_proxy = %s;\n"
                       "    if (%s_add_listener(%s, &%s_test_listener, adapter) != 0) {\n"
                       "        adapter->event_snapshot_failed = true;\n"
                       "        return;\n"
                       "    }\n"
                       "    adapter->%s_listener_installed = true;\n",
                       childArg->name.constData(), childPrefix.constData(),
                       childPrefix.constData(), childArg->name.constData(),
                       childIface->name.constData(), childArg->name.constData(),
                       childPrefix.constData(),
                       childPrefix.constData());
            } else {
                printf("    (void)%s;\n", childArg->name.constData());
            }
        }
        else if (needsPerEventStructs(*target)) {
            printf("    if (adapter->%s_event_count >= %s) {\n"
                   "        adapter->event_snapshot_failed = true;\n"
                   "        return;\n"
                   "    }\n"
                   "    struct %s_%s_event *e = "
                   "&adapter->%s_events[adapter->%s_event_count];\n",
                   event.name.constData(), maxEvents.constData(),
                   adapter.constData(), event.name.constData(),
                   event.name.constData(), event.name.constData());
            for (const WaylandArgument &argument : event.arguments) {
                if (argument.type == "string")
                    printf("    e->%s = %s ? strdup(%s) : NULL;\n",
                           argument.name.constData(), argument.name.constData(),
                           argument.name.constData());
                else
                    printf("    e->%s = %s;\n",
                           argument.name.constData(), argument.name.constData());
            }
            printf("    adapter->%s_event_count++;\n", event.name.constData());
        }
        else {
            for (const WaylandArgument &argument : event.arguments)
                printf("    (void)%s;\n", argument.name.constData());
        }
        printf("}\n\n");
    }
    if (!target->events.empty()) {
        printf("static const struct %s_listener test_listener = {\n", target->name.constData());
        for (const WaylandEvent &event : target->events)
            printf("    .%s = handle_%s,\n", event.name.constData(), event.name.constData());
        printf("};\n\n");
    }
    printf("static void handle_global(void *data, struct wl_registry *registry, uint32_t name, "
           "const char *interface, uint32_t version)\n{\n");
    printf("    struct %s_adapter *adapter = data;\n    (void)registry;\n", adapter.constData());
    printf("    if (strcmp(interface, %s_interface.name) != 0)\n        return;\n",
           target->name.constData());
    printf("    adapter->global_name = name;\n    adapter->advertised_version = version;\n}\n\n");
    printf("static void handle_global_remove(void *data, struct wl_registry *registry, uint32_t name)\n"
           "{\n    struct %s_adapter *adapter = data;\n    (void)registry;\n"
           "    if (adapter->global_name == name)\n        adapter->global_name = 0;\n}\n\n",
           adapter.constData());
    printf("static const struct wl_registry_listener registry_listener = {\n"
           "    .global = handle_global,\n    .global_remove = handle_global_remove,\n};\n\n");
    printf("void %s_adapter_init(struct %s_adapter *adapter)\n{\n    memset(adapter, 0, sizeof(*adapter));\n}\n\n",
           adapter.constData(), adapter.constData());
    printf("void %s_adapter_fini(struct %s_adapter *adapter)\n{\n"
           "    %s_clear_events(adapter);\n}\n\n",
           adapter.constData(), adapter.constData(), adapter.constData());
    printf("const struct wl_registry_listener *%s_registry_listener(void)\n{\n"
           "    return &registry_listener;\n}\n\n", adapter.constData());
    printf("int %s_bind(struct %s_adapter *adapter, struct wl_registry *registry, uint32_t requested_version)\n{\n",
           adapter.constData(), adapter.constData());
    printf("    if (!adapter->global_name || !adapter->advertised_version || adapter->proxy)\n        return -1;\n"
           "    adapter->bound_version = requested_version < adapter->advertised_version\n"
           "        ? requested_version : adapter->advertised_version;\n"
           "    adapter->proxy = wl_registry_bind(registry, adapter->global_name, &%s_interface, adapter->bound_version);\n"
           "    if (!adapter->proxy)\n        return -1;\n",
           target->name.constData());
    if (!target->events.empty()) {
        printf("#ifndef TL_TEST_ADAPTER_DISABLE_LISTENER\n"
               "    if (%s_add_listener(adapter->proxy, &test_listener, adapter) != 0) {\n"
               "        wl_proxy_destroy((struct wl_proxy *)adapter->proxy);\n"
               "        adapter->proxy = NULL;\n        return -1;\n    }\n"
               "#endif\n",
               target->name.constData());
    }
    printf("    adapter->local_proxy_alive = true;\n    return 0;\n}\n\n");
    printf("void %s_clear_events(struct %s_adapter *adapter)\n{\n"
           "    for (size_t i = 0; i < adapter->array_event_count; ++i) {\n"
           "        free(adapter->array_events[i].data);\n"
           "        adapter->array_events[i].data = NULL;\n"
           "        adapter->array_events[i].size = 0;\n"
           "    }\n"
           "    for (size_t i = 0; i < adapter->fd_event_count; ++i) {\n"
           "        if (adapter->fd_events[i].fd >= 0) {\n"
           "            close(adapter->fd_events[i].fd);\n"
           "            adapter->fd_events[i].fd = -1;\n"
           "        }\n"
           "    }\n",
           adapter.constData(), adapter.constData());
    if (needsPerEventStructs(*target)) {
        for (const WaylandEvent &event : target->events) {
            for (const WaylandArgument &argument : event.arguments) {
                if (perEventFieldNeedsCleanup(argument))
                    printf("    for (size_t i = 0; i < adapter->%s_event_count; ++i)\n"
                           "        free((void *)adapter->%s_events[i].%s);\n",
                           event.name.constData(), event.name.constData(),
                           argument.name.constData());
            }
            printf("    adapter->%s_event_count = 0;\n", event.name.constData());
        }
    }
    printf("    adapter->event_count = 0;\n"
           "    adapter->fixed_event_count = 0;\n"
           "    adapter->array_event_count = 0;\n"
           "    adapter->fd_event_count = 0;\n");
    for (const WaylandInterface *child : children) {
        const QByteArray prefix = childAdapterPrefix(child->name);
        printf("    if (adapter->%s_proxy) {\n"
               "        wl_proxy_destroy((struct wl_proxy *)adapter->%s_proxy);\n"
               "        adapter->%s_proxy = NULL;\n"
               "        adapter->%s_listener_installed = false;\n"
               "    }\n"
               "    adapter->%s_event_count = 0;\n",
               prefix.constData(), prefix.constData(), prefix.constData(),
               prefix.constData(), prefix.constData());
    }
    printf("    adapter->event_snapshot_failed = false;\n}\n");

    for (const WaylandEvent &request : target->requests) {
        bool hasNewId = false;
        for (const WaylandArgument &arg : request.arguments)
            if (arg.type == "new_id") hasNewId = true;

        printf("\nint %s_%s(struct %s_adapter *adapter", adapter.constData(), request.name.constData(), adapter.constData());
        for (const WaylandArgument &argument : request.arguments) {
            if (argument.type == "new_id")
                continue;
            const QByteArray type = testClientCType(argument);
            printf(", %s%s%s", type.constData(), type.endsWith('*') ? "" : " ", argument.name.constData());
        }
        printf(")\n{\n    if (!adapter->proxy || !adapter->local_proxy_alive)\n        return -1;\n");
        if (request.since > 1)
            printf("    if (adapter->bound_version < %d)\n        return -1;\n", request.since);
        for (const WaylandArgument &argument : request.arguments) {
            if (argument.type == "array" && !argument.allowNull)
                printf("    if (!%s)\n        return -1;\n", argument.name.constData());
        }
        if (hasNewId) {
            for (const WaylandArgument &argument : request.arguments) {
                if (argument.type == "new_id") {
                    QByteArray retType = argument.interface.isEmpty()
                        ? QByteArray("struct wl_proxy *")
                        : QByteArray("struct ") + argument.interface + " *";
                    printf("    %s %s = ", retType.constData(), argument.name.constData());
                }
            }
        }
        printf("    %s_%s(adapter->proxy", target->name.constData(), request.name.constData());
        for (const WaylandArgument &argument : request.arguments) {
            if (argument.type == "new_id")
                continue;
            printf(", %s", argument.name.constData());
        }
        printf(");\n");
        if (hasNewId) {
            for (const WaylandArgument &argument : request.arguments) {
                if (argument.type == "new_id")
                    printf("    if (!%s)\n        return -1;\n", argument.name.constData());
            }
        }
        if (request.type == "destructor")
            printf("    adapter->proxy = NULL;\n    adapter->local_proxy_alive = false;\n"
                   "    adapter->protocol_destructor_sent = true;\n");
        printf("    return 0;\n}\n");
    }

    // Generate generic dispatch function (uniform signature for runner)
    printf("\nint %s_dispatch(void *adapter_ptr, const char *name, const char **args, int arg_count)\n{\n",
           adapter.constData());
    printf("    struct %s_adapter *adapter = (struct %s_adapter *)adapter_ptr;\n",
           adapter.constData(), adapter.constData());
    for (const WaylandEvent &request : target->requests) {
        if (request.type == "destructor")
            continue;
        printf("    if (strcmp(name, \"%s\") == 0) {\n", request.name.constData());
        printf("        if (arg_count != %d)\n            return -1;\n",
               (int)request.arguments.size());
        // Parse each arg from string
        for (size_t i = 0; i < request.arguments.size(); ++i) {
            const WaylandArgument &arg = request.arguments[i];
            if (arg.type == "new_id" || arg.type == "object" || arg.type == "array"
                || arg.type == "fd" || arg.type == "fixed")
                continue;
            if (arg.type == "uint" || arg.type == "enum")
                printf("        uint32_t a_%s = (uint32_t)strtoul(args[%zu], NULL, 10);\n",
                       arg.name.constData(), i);
            else if (arg.type == "int")
                printf("        int32_t a_%s = (int32_t)strtol(args[%zu], NULL, 10);\n",
                       arg.name.constData(), i);
            else if (arg.type == "string")
                printf("        const char *a_%s = (arg_count > %zu && args[%zu]) ? args[%zu] : NULL;\n",
                       arg.name.constData(), i, i, i);
            else
                printf("        // TODO: parse type %s\n", arg.type.constData());
        }
        printf("        return %s_%s(adapter", adapter.constData(), request.name.constData());
        for (const WaylandArgument &arg : request.arguments) {
            if (arg.type == "new_id")
                continue;
            if (arg.type == "object" || arg.type == "array" || arg.type == "fd")
                printf(", NULL");
            else if (arg.type == "fixed")
                printf(", 0");
            else
                printf(", a_%s", arg.name.constData());
        }
        printf(");\n    }\n");
    }
    printf("    return -1;\n}\n");

    // Generate uniform registry struct for runner dispatch
    printf("\nconst struct %s_registry_type %s_registry = {\n",
           adapter.constData(), adapter.constData());
    printf("    .protocol_name = \"%s\",\n", target->name.constData());
    printf("    .adapter_size = sizeof(struct %s_adapter),\n", adapter.constData());
    printf("    .init = (void (*)(void *))%s_adapter_init,\n", adapter.constData());
    printf("    .fini = (void (*)(void *))%s_adapter_fini,\n", adapter.constData());
    printf("    .bind = (int (*)(void *, struct wl_registry *, uint32_t))%s_bind,\n", adapter.constData());
    printf("    .clear_events = (void (*)(void *))%s_clear_events,\n", adapter.constData());
    printf("    .dispatch = (int (*)(void *, const char *, const char **, int))%s_dispatch,\n", adapter.constData());
    printf("    .destroy = (int (*)(void *))%s_destroy,\n", adapter.constData());
    printf("    .listener = %s_registry_listener,\n", adapter.constData());
    printf("};\n");
}

void Scanner::printTestClientMetadata(const std::vector<WaylandInterface> &interfaces)
{
    QJsonArray interfaceArray;
    for (const WaylandInterface &interface : interfaces) {
        if (ignoreInterface(interface.name))
            continue;
        auto operations = [](const std::vector<WaylandEvent> &events) {
            QJsonArray result;
            for (const WaylandEvent &event : events) {
                QJsonArray arguments;
                for (const WaylandArgument &argument : event.arguments) {
                    arguments.append(QJsonObject{
                        { QStringLiteral("name"), QString::fromUtf8(argument.name) },
                        { QStringLiteral("type"), QString::fromUtf8(argument.type) },
                        { QStringLiteral("interface"), argument.interface.isEmpty()
                              ? QJsonValue(QJsonValue::Null)
                              : QJsonValue(QString::fromUtf8(argument.interface)) },
                        { QStringLiteral("allow_null"), argument.allowNull },
                    });
                }
                result.append(QJsonObject{
                    { QStringLiteral("name"), QString::fromUtf8(event.name) },
                    { QStringLiteral("destructor"), event.type == "destructor" },
                    { QStringLiteral("since"), event.since },
                    { QStringLiteral("arguments"), arguments },
                });
            }
            return result;
        };
        QJsonArray enumArray;
        for (const WaylandEnum &e : interface.enums) {
            QJsonArray entries;
            for (const WaylandEnumEntry &entry : e.entries) {
                entries.append(QJsonObject{
                    { QStringLiteral("name"), QString::fromUtf8(entry.name) },
                    { QStringLiteral("value"), QString::fromUtf8(entry.value) },
                });
            }
            enumArray.append(QJsonObject{
                { QStringLiteral("name"), QString::fromUtf8(e.name) },
                { QStringLiteral("entries"), entries },
            });
        }
        interfaceArray.append(QJsonObject{
            { QStringLiteral("name"), QString::fromUtf8(interface.name) },
            { QStringLiteral("version"), interface.version },
            { QStringLiteral("requests"), operations(interface.requests) },
            { QStringLiteral("events"), operations(interface.events) },
            { QStringLiteral("enums"), enumArray },
        });
    }
    const QJsonObject root{
        { QStringLiteral("protocol"), QString::fromUtf8(m_protocolName) },
        { QStringLiteral("interfaces"), interfaceArray },
    };
    const QByteArray json = QJsonDocument(root).toJson(QJsonDocument::Indented);
    fwrite(json.constData(), 1, static_cast<size_t>(json.size()), stdout);
}

bool Scanner::process()
{
    QFile file(m_protocolFilePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        fprintf(stderr, "Unable to open file %s\n", m_protocolFilePath.constData());
        return false;
    }

    m_xml = new QXmlStreamReader(&file);
    if (!m_xml->readNextStartElement())
        return false;

    if (m_xml->name() != QLatin1String("protocol")) {
        m_xml->raiseError(QStringLiteral("The file is not a wayland protocol file."));
        return false;
    }

    m_protocolName = byteArrayValue(*m_xml, "name");

    if (m_protocolName.isEmpty()) {
        m_xml->raiseError(QStringLiteral("Missing protocol name."));
        return false;
    }

    // We should convert - to _ so that the preprocessor wont generate code which will lead to
    // unexpected behavior However, the wayland-scanner doesn't do so we will do the same for now
    // QByteArray preProcessorProtocolName = QByteArray(m_protocolName).replace('-', '_').toUpper();
    QByteArray preProcessorProtocolName = QByteArray(m_protocolName).toUpper();

    std::vector<WaylandInterface> interfaces;

    while (m_xml->readNextStartElement()) {
        if (m_xml->name() == QLatin1String("interface"))
            interfaces.push_back(readInterface(*m_xml));
        else
            m_xml->skipCurrentElement();
    }

    if (m_xml->hasError())
        return false;

    if (m_option == TestClientMetadata) {
        printTestClientMetadata(interfaces);
        return true;
    }

    printf("// This file was generated by qtwaylandscanner\n");
    printf("// source file is %s\n\n", qPrintable(m_protocolFilePath));

    for (auto b : std::as_const(m_includes))
        printf("#include %s\n", b.constData());

    if (m_option == TestClientHeader) {
        printTestClientHeader(interfaces);
        return true;
    }
    if (m_option == TestClientCode) {
        printTestClientCode(interfaces);
        return true;
    }

    if (m_option == ServerHeader) {
        QByteArray inclusionGuard =
            QByteArray("QT_WAYLAND_SERVER_") + preProcessorProtocolName.constData();
        printf("#ifndef %s\n", inclusionGuard.constData());
        printf("#define %s\n", inclusionGuard.constData());
        printf("\n");
        printf("#include \"wayland-server-core.h\"\n");
        if (m_headerPath.isEmpty())
            printf("#include \"wayland-%s-server-protocol.h\"\n",
                   QByteArray(m_protocolName).replace('_', '-').constData());
        else
            printf("#include <%s/wayland-%s-server-protocol.h>\n",
                   m_headerPath.constData(),
                   QByteArray(m_protocolName).replace('_', '-').constData());
        printf("#include <QByteArray>\n");
        printf("#include <QMultiMap>\n");
        printf("#include <QString>\n");

        printf("\n");
        printf("#include <unistd.h>\n");

        printf("\n");
        printf("#ifndef WAYLAND_VERSION_CHECK\n");
        printf("#define WAYLAND_VERSION_CHECK(major, minor, micro) \\\n");
        printf("    ((WAYLAND_VERSION_MAJOR > (major)) || \\\n");
        printf("    (WAYLAND_VERSION_MAJOR == (major) && WAYLAND_VERSION_MINOR > (minor)) || \\\n");
        printf("    (WAYLAND_VERSION_MAJOR == (major) && WAYLAND_VERSION_MINOR == (minor) && "
               "WAYLAND_VERSION_MICRO >= (micro)))\n");
        printf("#endif\n");

        printf("\n");
        printf("QT_BEGIN_NAMESPACE\n");
        printf("QT_WARNING_PUSH\n");
        printf("QT_WARNING_DISABLE_GCC(\"-Wmissing-field-initializers\")\n");
        printf("QT_WARNING_DISABLE_CLANG(\"-Wmissing-field-initializers\")\n");
        printf("QT_WARNING_DISABLE_GCC(\"-Wreorder\")\n");
        printf("QT_WARNING_DISABLE_CLANG(\"-Wreorder\")\n");
        QByteArray serverExport;
        if (m_headerPath.size()) {
            serverExport = QByteArray("Q_WAYLAND_SERVER_") + preProcessorProtocolName + "_EXPORT";
            printf("\n");
            printf("#if !defined(%s)\n", serverExport.constData());
            printf("#  if defined(QT_SHARED)\n");
            printf("#    define %s Q_DECL_EXPORT\n", serverExport.constData());
            printf("#  else\n");
            printf("#    define %s\n", serverExport.constData());
            printf("#  endif\n");
            printf("#endif\n");
        }
        printf("\n");
        printf("namespace QtWaylandServer {\n");

        bool needsNewLine = false;
        for (const WaylandInterface &interface : interfaces) {

            if (ignoreInterface(interface.name))
                continue;

            if (needsNewLine)
                printf("\n");
            needsNewLine = true;

            const char *interfaceName = interface.name.constData();

            QByteArray stripped = stripInterfaceName(interface.name);
            const char *interfaceNameStripped = stripped.constData();

            printf("    class %s %s\n    {\n", serverExport.constData(), interfaceName);
            printf("    public:\n");
            printf("        %s(struct ::wl_client *client, int id, int version);\n", interfaceName);
            printf("        %s(struct ::wl_display *display, int version);\n", interfaceName);
            printf("        %s(struct ::wl_resource *resource);\n", interfaceName);
            printf("        %s();\n", interfaceName);
            printf("\n");
            printf("        virtual ~%s();\n", interfaceName);
            printf("\n");
            printf("        class Resource\n");
            printf("        {\n");
            printf("        public:\n");
            printf("            Resource() : %s_object(nullptr), handle(nullptr) {}\n",
                   interfaceNameStripped);
            printf("            virtual ~Resource() {}\n");
            printf("\n");
            printf("            %s *%s_object;\n", interfaceName, interfaceNameStripped);
            printf("            %s *object() { return %s_object; } \n",
                   interfaceName,
                   interfaceNameStripped);
            printf("            struct ::wl_resource *handle;\n");
            printf("\n");
            printf("            struct ::wl_client *client() const { return "
                   "wl_resource_get_client(handle); }\n");
            printf("            int version() const { return wl_resource_get_version(handle); }\n");
            printf("\n");
            printf("            static Resource *fromResource(struct ::wl_resource *resource);\n");
            printf("        };\n");
            printf("\n");
            printf("        void init(struct ::wl_client *client, int id, int version);\n");
            printf("        void init(struct ::wl_display *display, int version);\n");
            printf("        void init(struct ::wl_resource *resource);\n");
            printf("\n");
            printf("        Resource *add(struct ::wl_client *client, int version);\n");
            printf("        Resource *add(struct ::wl_client *client, int id, int version);\n");
            printf("        Resource *add(struct wl_list *resource_list, struct ::wl_client "
                   "*client, int id, int version);\n");
            printf("\n");
            printf("        Resource *resource() { return m_resource; }\n");
            printf("        const Resource *resource() const { return m_resource; }\n");
            printf("\n");
            printf("        const QMultiMap<struct ::wl_client*, Resource*> &resourceMap() const { "
                   "return m_resource_map; }\n");
            printf("\n");
            printf("        bool isGlobalRemoved() const { return m_globalRemovedEvent; }\n");
            printf("        void globalRemove();\n");
            printf("\n");
            printf("        bool isGlobal() const { return m_global != nullptr; }\n");
            printf("        bool isResource() const { return m_resource != nullptr; }\n");
            printf("\n");
            printf("        static const struct ::wl_interface *interface();\n");
            printf("        static QByteArrayView interfaceName() { return interface()->name; }\n");
            printf("        static int interfaceVersion() { return interface()->version; }\n");
            printf("\n");

            printEnums(interface.enums);

            bool hasEvents = !interface.events.empty();

            if (hasEvents) {
                printf("\n");
                for (const WaylandEvent &e : interface.events) {
                    printf("        void send_");
                    printEvent(e);
                    printf(";\n");
                    printf("        void send_");
                    printEvent(e, false, true);
                    printf(";\n");
                }
            }

            printf("    protected:\n");
            printf("        struct ::wl_global *m_global;\n");
            printf("\n");

            printf("    protected:\n");
            printf("        virtual Resource *allocate();\n");
            printf("\n");
            printf("        virtual void destroy_global();\n");
            printf("\n");
                 printf("        virtual void bind_resource(Resource *resource);\n");
                 printf("        virtual void destroy_resource(Resource *resource);\n");

            bool hasRequests = !interface.requests.empty();

            if (hasRequests) {
                printf("\n");
                for (const WaylandEvent &e : interface.requests) {
                    printf("        virtual void ");
                    printEvent(e);
                    printf(";\n");
                }
            }

            printf("\n");
            printf("    private:\n");
            printf("        static void bind_func(struct ::wl_client *client, void *data, uint32_t "
                   "version, uint32_t id);\n");
            printf("        static void destroy_func(struct ::wl_resource *client_resource);\n");
            printf("        static void display_destroy_func(struct ::wl_listener *listener, void "
                   "*data);\n");
            printf("        static int deferred_destroy_global_func(void *data);\n");
            printf("\n");
            printf(
                "        Resource *bind(struct ::wl_client *client, uint32_t id, int version);\n");
            printf("        Resource *bind(struct ::wl_resource *handle);\n");

            if (hasRequests) {
                printf("\n");
                printf("        static const struct ::%s_interface m_%s_interface;\n",
                       interfaceName,
                       interfaceName);

                printf("\n");
                for (const WaylandEvent &e : interface.requests) {
                    printf("        static void ");

                    printEventHandlerSignature(e, interfaceName);
                    printf(";\n");
                }
            }

            printf("\n");
            printf("        QMultiMap<struct ::wl_client*, Resource*> m_resource_map;\n");
            printf("        Resource *m_resource;\n");
            printf("        struct ::wl_display *m_display;\n");
            printf("        struct wl_event_source *m_globalRemovedEvent;\n");
            printf("        struct DisplayDestroyedListener : ::wl_listener {\n");
            printf("            %s *parent;\n", interfaceName);
            printf("        };\n");
            printf("        DisplayDestroyedListener m_displayDestroyedListener;\n");
            printf("    };\n");
        }

        printf("}\n");
        printf("\n");
        printf("QT_WARNING_POP\n");
        printf("QT_END_NAMESPACE\n");
        printf("\n");
        printf("#endif\n");
    }

    if (m_option == ServerCode) {
        if (m_headerPath.isEmpty())
            printf("#include \"qwayland-server-%s.h\"\n",
                   QByteArray(m_protocolName).replace('_', '-').constData());
        else
            printf("#include <%s/qwayland-server-%s.h>\n",
                   m_headerPath.constData(),
                   QByteArray(m_protocolName).replace('_', '-').constData());
        printf("\n");
        printf("QT_BEGIN_NAMESPACE\n");
        printf("QT_WARNING_PUSH\n");
        printf("QT_WARNING_DISABLE_GCC(\"-Wmissing-field-initializers\")\n");
        printf("QT_WARNING_DISABLE_GCC(\"-Wreorder\")\n");
        printf("QT_WARNING_DISABLE_CLANG(\"-Wreorder\")\n");
        printf("\n");
        printf("namespace QtWaylandServer {\n");

        bool needsNewLine = false;

        for (const WaylandInterface &interface : interfaces) {

            if (ignoreInterface(interface.name))
                continue;

            if (needsNewLine)
                printf("\n");

            needsNewLine = true;

            const char *interfaceName = interface.name.constData();

            QByteArray stripped = stripInterfaceName(interface.name);
            const char *interfaceNameStripped = stripped.constData();

            printf("\n");
            printf("    int %s::deferred_destroy_global_func(void *data) {\n", interfaceName);
            printf("        auto object = static_cast<%s *>(data);\n", interfaceName);
            printf("        wl_global_destroy(object->m_global);\n");
            printf("        object->m_global = nullptr;\n");
            printf("        wl_event_source_remove(object->m_globalRemovedEvent);\n");
            printf("        object->m_globalRemovedEvent = nullptr;\n");
            printf("        wl_list_remove(&object->m_displayDestroyedListener.link);\n");
            printf("        object->destroy_global();\n");
            printf("        return 0;\n");
            printf("    }\n");
            printf("\n");

            printf("    %s::%s(struct ::wl_client *client, int id, int version)\n",
                   interfaceName,
                   interfaceName);
            printf("        : m_resource_map()\n");
            printf("        , m_resource(nullptr)\n");
            printf("        , m_global(nullptr)\n");
            printf("        , m_display(nullptr)\n");
            printf("        , m_globalRemovedEvent(nullptr)\n");
            printf("    {\n");
            printf("        init(client, id, version);\n");
            printf("    }\n");
            printf("\n");

            printf("    %s::%s(struct ::wl_display *display, int version)\n",
                   interfaceName,
                   interfaceName);
            printf("        : m_resource_map()\n");
            printf("        , m_resource(nullptr)\n");
            printf("        , m_global(nullptr)\n");
            printf("        , m_display(nullptr)\n");
            printf("        , m_globalRemovedEvent(nullptr)\n");
            printf("    {\n");
            printf("        init(display, version);\n");
            printf("    }\n");
            printf("\n");

            printf("    %s::%s(struct ::wl_resource *resource)\n", interfaceName, interfaceName);
            printf("        : m_resource_map()\n");
            printf("        , m_resource(nullptr)\n");
            printf("        , m_global(nullptr)\n");
            printf("        , m_display(nullptr)\n");
            printf("        , m_globalRemovedEvent(nullptr)\n");
            printf("    {\n");
            printf("        init(resource);\n");
            printf("    }\n");
            printf("\n");

            printf("    %s::%s()\n", interfaceName, interfaceName);
            printf("        : m_resource_map()\n");
            printf("        , m_resource(nullptr)\n");
            printf("        , m_global(nullptr)\n");
            printf("        , m_display(nullptr)\n");
            printf("        , m_globalRemovedEvent(nullptr)\n");
            printf("    {\n");
            printf("    }\n");
            printf("\n");

            printf("    %s::~%s()\n", interfaceName, interfaceName);
            printf("    {\n");
            printf("        for (auto resource : std::as_const(m_resource_map))\n");
            printf("            resource->%s_object = nullptr;\n", interfaceNameStripped);
            printf("\n");
            printf("        if (m_resource)\n");
            printf("            m_resource->%s_object = nullptr;\n", interfaceNameStripped);
            printf("\n");
            printf("        if (m_global) {\n");
            printf("            if (m_globalRemovedEvent)\n");
            printf("                wl_event_source_remove(m_globalRemovedEvent);\n");
            printf("            wl_global_destroy(m_global);\n");
            printf("            wl_list_remove(&m_displayDestroyedListener.link);\n");
            printf("        }\n");
            printf("    }\n");
            printf("\n");

            printf("    void %s::init(struct ::wl_client *client, int id, int version)\n",
                   interfaceName);
            printf("    {\n");
            printf("        m_resource = bind(client, id, version);\n");
            printf("    }\n");
            printf("\n");

            printf("    void %s::init(struct ::wl_resource *resource)\n", interfaceName);
            printf("    {\n");
            printf("        m_resource = bind(resource);\n");
            printf("    }\n");
            printf("\n");

            printf("    %s::Resource *%s::add(struct ::wl_client *client, int version)\n",
                   interfaceName,
                   interfaceName);
            printf("    {\n");
            printf("        Resource *resource = bind(client, 0, version);\n");
            printf("        m_resource_map.insert(client, resource);\n");
            printf("        return resource;\n");
            printf("    }\n");
            printf("\n");

            printf("    %s::Resource *%s::add(struct ::wl_client *client, int id, int version)\n",
                   interfaceName,
                   interfaceName);
            printf("    {\n");
            printf("        Resource *resource = bind(client, id, version);\n");
            printf("        m_resource_map.insert(client, resource);\n");
            printf("        return resource;\n");
            printf("    }\n");
            printf("\n");

            printf("    void %s::init(struct ::wl_display *display, int version)\n", interfaceName);
            printf("    {\n");
            printf("        m_display = display;\n");
            printf("        m_global = wl_global_create(display, &::%s_interface, version, this, "
                   "bind_func);\n",
                   interfaceName);
            printf("        m_displayDestroyedListener.notify = %s::display_destroy_func;\n",
                   interfaceName);
            printf("        m_displayDestroyedListener.parent = this;\n");
            printf(
                "        wl_display_add_destroy_listener(display, &m_displayDestroyedListener);\n");
            printf("    }\n");
            printf("\n");

            printf("    const struct wl_interface *%s::interface()\n", interfaceName);
            printf("    {\n");
            printf("        return &::%s_interface;\n", interfaceName);
            printf("    }\n");
            printf("\n");

                 printf("    %s::Resource *%s::allocate()\n",
                     interfaceName,
                     interfaceName);
            printf("    {\n");
            printf("        return new Resource;\n");
            printf("    }\n");
            printf("\n");

            printf("    void %s::destroy_global()\n", interfaceName);
            printf("    {\n");
            printf("    }\n");
            printf("\n");

                 printf("    void %s::bind_resource(Resource *)\n",
                     interfaceName);
            printf("    {\n");
            printf("    }\n");
            printf("\n");

                 printf("    void %s::destroy_resource(Resource *)\n",
                     interfaceName);
            printf("    {\n");
            printf("    }\n");
            printf("\n");

            printf("    void %s::bind_func(struct ::wl_client *client, void *data, uint32_t "
                   "version, uint32_t id)\n",
                   interfaceName);
            printf("    {\n");
            printf("        %s *that = static_cast<%s *>(data);\n", interfaceName, interfaceName);
            printf("        that->add(client, id, version);\n");
            printf("    }\n");
            printf("\n");

            printf(
                "    void %s::display_destroy_func(struct ::wl_listener *listener, void *data)\n",
                interfaceName);
            printf("    {\n");
            printf("        Q_UNUSED(data);\n");
            printf("        %s *that = static_cast<%s::DisplayDestroyedListener "
                   "*>(listener)->parent;\n",
                   interfaceName,
                   interfaceName);
            printf("        that->m_global = nullptr;\n");
            printf("        that->m_globalRemovedEvent = nullptr;\n");
            printf("    }\n");
            printf("\n");

            printf("    void %s::destroy_func(struct ::wl_resource *client_resource)\n",
                   interfaceName);
            printf("    {\n");
            printf("        Resource *resource = Resource::fromResource(client_resource);\n");
            printf("        Q_ASSERT(resource);\n");
            printf("        %s *that = resource->%s_object;\n",
                   interfaceName,
                   interfaceNameStripped);
            printf("        if (Q_LIKELY(that)) {\n");
            printf("            that->m_resource_map.remove(resource->client(), resource);\n");
            printf("            that->destroy_resource(resource);\n");
            printf("\n");
            printf("            that = resource->%s_object;\n", interfaceNameStripped);
            printf("            if (that && that->m_resource == resource)\n");
            printf("                that->m_resource = nullptr;\n");
            printf("        }\n");
            printf("        delete resource;\n");
            printf("    }\n");
            printf("\n");

            // Removing a global is racey. Announce removal, and then only perform internal cleanup
            // after a delay See https://gitlab.freedesktop.org/wayland/wayland/issues/10
            printf("\n");
            printf("    void %s::globalRemove()\n", interfaceName);
            printf("    {\n");
            printf("        if (!m_global || m_globalRemovedEvent)\n");
            printf("            return;\n");
            printf("\n");
            printf("        wl_global_remove(m_global);\n");
            printf("\n");
            printf("        struct wl_event_loop *event_loop = "
                   "wl_display_get_event_loop(m_display);\n");
            printf("        m_globalRemovedEvent = wl_event_loop_add_timer(event_loop, "
                   "deferred_destroy_global_func, this);\n");
            printf("        wl_event_source_timer_update(m_globalRemovedEvent, 5000);\n");
            printf("    }\n");
            printf("\n");

            bool hasRequests = !interface.requests.empty();

            QByteArray interfaceMember =
                hasRequests ? "&m_" + interface.name + "_interface" : QByteArray("nullptr");

            // We should consider changing bind so that it doesn't special case id == 0
            // and use function overloading instead. Jan do you have a lot of code dependent on this
            // behavior?
            printf("    %s::Resource *%s::bind(struct ::wl_client *client, uint32_t id, int "
                   "version)\n",
                   interfaceName,
                   interfaceName);
            printf("    {\n");
            printf("        Q_ASSERT_X(!wl_client_get_object(client, id), \"QWaylandObject bind\", "
                   "QStringLiteral(\"binding to object %%1 more than "
                   "once\").arg(id).toLocal8Bit().constData());\n");
            printf("        struct ::wl_resource *handle = wl_resource_create(client, "
                   "&::%s_interface, version, id);\n",
                   interfaceName);
            printf("        return bind(handle);\n");
            printf("    }\n");
            printf("\n");

            printf("    %s::Resource *%s::bind(struct ::wl_resource *handle)\n",
                   interfaceName,
                   interfaceName);
            printf("    {\n");
            printf("        Resource *resource = allocate();\n");
            printf("        resource->%s_object = this;\n", interfaceNameStripped);
            printf("\n");
            printf("        wl_resource_set_implementation(handle, %s, resource, destroy_func);",
                   interfaceMember.constData());
            printf("\n");
            printf("        resource->handle = handle;\n");
            printf("        bind_resource(resource);\n");
            printf("        return resource;\n");
            printf("    }\n");

            printf("    %s::Resource *%s::Resource::fromResource(struct ::wl_resource *resource)\n",
                   interfaceName,
                   interfaceName);
            printf("    {\n");
            printf("        if (Q_UNLIKELY(!resource))\n");
            printf("            return nullptr;\n");
            printf("        if (wl_resource_instance_of(resource, &::%s_interface, %s))\n",
                   interfaceName,
                   interfaceMember.constData());
            printf("            return static_cast<Resource "
                   "*>(wl_resource_get_user_data(resource));\n");
            printf("        return nullptr;\n");
            printf("    }\n");

            if (hasRequests) {
                printf("\n");
                printf("    const struct ::%s_interface %s::m_%s_interface = {",
                       interfaceName,
                       interfaceName,
                       interfaceName);
                bool needsComma = false;
                for (const WaylandEvent &e : interface.requests) {
                    if (needsComma)
                        printf(",");
                    needsComma = true;
                    printf("\n");
                    printf("        %s::handle_%s", interfaceName, e.name.constData());
                }
                printf("\n");
                printf("    };\n");

                for (const WaylandEvent &e : interface.requests) {
                    printf("\n");
                    printf("    void %s::", interfaceName);
                    printEvent(e);
                    printf("\n");
                    printf("    {\n");
                    if (isServerSide()) {
                        if (e.request) {
                            printf("        Q_UNUSED(resource);\n");
                        }
                    }
                    for (const WaylandArgument &a : e.arguments) {
                        bool isNewId = a.type == "new_id";
                        if (isServerSide() && isNewId && e.request)
                            printf("        Q_UNUSED(%s);\n", a.name.constData());
                        else if (!isNewId || !isServerSide() || !e.request)
                            printf("        Q_UNUSED(%s);\n", a.name.constData());
                    }
                    printf("    }\n");
                }
                printf("\n");

                for (const WaylandEvent &e : interface.requests) {
                    printf("\n");
                    printf("    void %s::", interfaceName);
                    printEventHandlerSignature(e, interfaceName, false);
                    printf("\n");
                    printf("    {\n");
                    printf("        Q_UNUSED(client);\n");
                    printf("        Resource *qResource = Resource::fromResource(resource);\n");
                    printf("        if (Q_UNLIKELY(!qResource->%s_object)) {\n", interfaceNameStripped);
                    for (const WaylandArgument &a : e.arguments) {
                        if (a.type == QByteArrayLiteral("fd"))
                            printf("        close(%s);\n", a.name.constData());
                    }
                    if (e.type == "destructor")
                        printf("            wl_resource_destroy(resource);\n");
                    printf("            return;\n");
                    printf("        }\n");
                    printf("        static_cast<%s *>(qResource->%s_object)->%s(\n",
                           interfaceName,
                           interfaceNameStripped,
                           e.name.constData());
                    printf("            qResource");
                    for (const WaylandArgument &a : e.arguments) {
                        printf(",\n");
                        QByteArray cType = waylandToCType(a.type, a.interface);
                        QByteArray qtType = waylandToQtType(a.type, a.interface, e.request);
                        const char *argumentName = a.name.constData();
                        if (cType == qtType)
                            printf("            %s", argumentName);
                        else if (a.type == "string")
                            printf("            QString::fromUtf8(%s)", argumentName);
                    }
                    printf(");\n");
                    printf("    }\n");
                }
            }

            for (const WaylandEvent &e : interface.events) {
                printf("\n");
                printf("    void %s::send_", interfaceName);
                printEvent(e);
                printf("\n");
                printf("    {\n");
                printf("        Q_ASSERT_X(m_resource, \"%s::%s\", \"Uninitialised resource\");\n",
                       interfaceName,
                       e.name.constData());
                printf("        if (Q_UNLIKELY(!m_resource)) {\n");
                printf("            qWarning(\"could not call %s::%s as it's not initialised\");\n",
                       interfaceName,
                       e.name.constData());
                printf("            return;\n");
                printf("        }\n");
                printf("        send_%s(\n", e.name.constData());
                printf("            m_resource->handle");
                for (const WaylandArgument &a : e.arguments) {
                    printf(",\n");
                    printf("            %s", a.name.constData());
                }
                printf(");\n");
                printf("    }\n");
                printf("\n");

                printf("    void %s::send_", interfaceName);
                printEvent(e, false, true);
                printf("\n");
                printf("    {\n");

                for (const WaylandArgument &a : e.arguments) {
                    if (a.type != "array")
                        continue;
                    QByteArray array = a.name + "_data";
                    const char *arrayName = array.constData();
                    const char *variableName = a.name.constData();
                    printf("        struct wl_array %s;\n", arrayName);
                    printf("        %s.size = %s.size();\n", arrayName, variableName);
                    printf("        %s.data = static_cast<void *>(const_cast<char "
                           "*>(%s.constData()));\n",
                           arrayName,
                           variableName);
                    printf("        %s.alloc = 0;\n", arrayName);
                    printf("\n");
                }

                printf("        %s_send_%s(\n", interfaceName, e.name.constData());
                printf("            resource");

                for (const WaylandArgument &a : e.arguments) {
                    printf(",\n");
                    QByteArray cType = waylandToCType(a.type, a.interface);
                    QByteArray qtType = waylandToQtType(a.type, a.interface, e.request);
                    if (a.type == "string")
                        printf("            %s.toUtf8().constData()", a.name.constData());
                    else if (a.type == "array")
                        printf("            &%s_data", a.name.constData());
                    else if (cType == qtType)
                        printf("            %s", a.name.constData());
                }

                printf(");\n");
                printf("    }\n");
                printf("\n");
            }
        }
        printf("}\n");
        printf("\n");
        printf("QT_WARNING_POP\n");
        printf("QT_END_NAMESPACE\n");
    }

    if (m_option == ClientHeader) {
        QByteArray inclusionGuard =
            QByteArray("QT_WAYLAND_") + preProcessorProtocolName.constData();
        printf("#ifndef %s\n", inclusionGuard.constData());
        printf("#define %s\n", inclusionGuard.constData());
        printf("\n");
        if (m_headerPath.isEmpty())
            printf("#include \"wayland-%s-client-protocol.h\"\n",
                   QByteArray(m_protocolName).replace('_', '-').constData());
        else
            printf("#include <%s/wayland-%s-client-protocol.h>\n",
                   m_headerPath.constData(),
                   QByteArray(m_protocolName).replace('_', '-').constData());
        printf("#include <QByteArray>\n");
        printf("#include <QString>\n");
        printf("\n");
        printf("struct wl_registry;\n");
        printf("\n");
        printf("QT_BEGIN_NAMESPACE\n");
        printf("QT_WARNING_PUSH\n");
        printf("QT_WARNING_DISABLE_GCC(\"-Wmissing-field-initializers\")\n");
        printf("QT_WARNING_DISABLE_GCC(\"-Wreorder\")\n");
        printf("QT_WARNING_DISABLE_CLANG(\"-Wreorder\")\n");

        QByteArray clientExport;

        if (m_headerPath.size()) {
            clientExport = QByteArray("Q_WAYLAND_CLIENT_") + preProcessorProtocolName + "_EXPORT";
            printf("\n");
            printf("#if !defined(%s)\n", clientExport.constData());
            printf("#  if defined(QT_SHARED)\n");
            printf("#    define %s Q_DECL_EXPORT\n", clientExport.constData());
            printf("#  else\n");
            printf("#    define %s\n", clientExport.constData());
            printf("#  endif\n");
            printf("#endif\n");
        }
        printf("\n");
        printf("namespace QtWayland {\n");

        bool needsNewLine = false;
        for (const WaylandInterface &interface : interfaces) {

            if (ignoreInterface(interface.name))
                continue;

            if (needsNewLine)
                printf("\n");
            needsNewLine = true;

            const char *interfaceName = interface.name.constData();

            printf("    class %s %s\n    {\n", clientExport.constData(), interfaceName);
            printf("    public:\n");
            printf("        %s(struct ::wl_registry *registry, int id, int version);\n",
                   interfaceName);
            printf("        %s(struct ::%s *object);\n", interfaceName, interfaceName);
            printf("        %s();\n", interfaceName);
            printf("\n");
            printf("        virtual ~%s();\n", interfaceName);
            printf("\n");
            printf("        void init(struct ::wl_registry *registry, int id, int version);\n");
            printf("        void init(struct ::%s *object);\n", interfaceName);
            printf("\n");
            printf("        struct ::%s *object() { return m_%s; }\n",
                   interfaceName,
                   interfaceName);
            printf("        const struct ::%s *object() const { return m_%s; }\n",
                   interfaceName,
                   interfaceName);
            printf("        static %s *fromObject(struct ::%s *object);\n",
                   interfaceName,
                   interfaceName);
            printf("\n");
            printf("        bool isInitialized() const;\n");
            printf("\n");
            printf("        static const struct ::wl_interface *interface();\n");

            printEnums(interface.enums);

            if (!interface.requests.empty()) {
                printf("\n");
                for (const WaylandEvent &e : interface.requests) {
                    const WaylandArgument *new_id = newIdArgument(e.arguments);
                    QByteArray new_id_str = "void ";
                    if (new_id) {
                        if (new_id->interface.isEmpty())
                            new_id_str = "void *";
                        else
                            new_id_str = "struct ::" + new_id->interface + " *";
                    }
                    printf("        %s", new_id_str.constData());
                    printEvent(e);
                    printf(";\n");
                }
            }

            bool hasEvents = !interface.events.empty();

            if (hasEvents) {
                printf("\n");
                printf("    protected:\n");
                for (const WaylandEvent &e : interface.events) {
                    printf("        virtual void ");
                    printEvent(e);
                    printf(";\n");
                }
            }

            printf("\n");
            printf("    private:\n");
            if (hasEvents) {
                printf("        void init_listener();\n");
                printf("        static const struct %s_listener m_%s_listener;\n",
                       interfaceName,
                       interfaceName);
                for (const WaylandEvent &e : interface.events) {
                    printf("        static void ");

                    printEventHandlerSignature(e, interfaceName);
                    printf(";\n");
                }
            }
            printf("        struct ::%s *m_%s;\n", interfaceName, interfaceName);
            printf("    };\n");
        }
        printf("}\n");
        printf("\n");
        printf("QT_WARNING_POP\n");
        printf("QT_END_NAMESPACE\n");
        printf("\n");
        printf("#endif\n");
    }

    if (m_option == ClientCode) {
        if (m_headerPath.isEmpty())
            printf("#include \"qwayland-%s.h\"\n",
                   QByteArray(m_protocolName).replace('_', '-').constData());
        else
            printf("#include <%s/qwayland-%s.h>\n",
                   m_headerPath.constData(),
                   QByteArray(m_protocolName).replace('_', '-').constData());
        printf("\n");
        printf("QT_BEGIN_NAMESPACE\n");
        printf("QT_WARNING_PUSH\n");
        printf("QT_WARNING_DISABLE_GCC(\"-Wmissing-field-initializers\")\n");
        printf("QT_WARNING_DISABLE_GCC(\"-Wreorder\")\n");
        printf("QT_WARNING_DISABLE_CLANG(\"-Wreorder\")\n");
        printf("\n");
        printf("namespace QtWayland {\n");
        printf("\n");

        // wl_registry_bind is part of the protocol, so we can't use that... instead we use core
        // libwayland API to do the same thing a wayland-scanner generated wl_registry_bind would.
        printf("static inline void *wlRegistryBind(struct ::wl_registry *registry, uint32_t name, "
               "const struct ::wl_interface *interface, uint32_t version)\n");
        printf("{\n");
        printf("    const uint32_t bindOpCode = 0;\n");
        printf("#if (WAYLAND_VERSION_MAJOR == 1 && WAYLAND_VERSION_MINOR > 10) || "
               "WAYLAND_VERSION_MAJOR > 1\n");
        printf("    return (void *) wl_proxy_marshal_constructor_versioned((struct wl_proxy *) "
               "registry,\n");
        printf(
            "        bindOpCode, interface, version, name, interface->name, version, nullptr);\n");
        printf("#else\n");
        printf("    return (void *) wl_proxy_marshal_constructor((struct wl_proxy *) registry,\n");
        printf("        bindOpCode, interface, name, interface->name, version, nullptr);\n");
        printf("#endif\n");
        printf("}\n");
        printf("\n");

        bool needsNewLine = false;
        for (const WaylandInterface &interface : interfaces) {

            if (ignoreInterface(interface.name))
                continue;

            if (needsNewLine)
                printf("\n");
            needsNewLine = true;

            const char *interfaceName = interface.name.constData();

            bool hasEvents = !interface.events.empty();

            printf("    %s::%s(struct ::wl_registry *registry, int id, int version)\n",
                   interfaceName,
                   interfaceName);
            printf("    {\n");
            printf("        init(registry, id, version);\n");
            printf("    }\n");
            printf("\n");

            printf("    %s::%s(struct ::%s *obj)\n", interfaceName, interfaceName, interfaceName);
            printf("        : m_%s(obj)\n", interfaceName);
            printf("    {\n");
            if (hasEvents)
                printf("        init_listener();\n");
            printf("    }\n");
            printf("\n");

            printf("    %s::%s()\n", interfaceName, interfaceName);
            printf("        : m_%s(nullptr)\n", interfaceName);
            printf("    {\n");
            printf("    }\n");
            printf("\n");

            printf("    %s::~%s()\n", interfaceName, interfaceName);
            printf("    {\n");
            printf("    }\n");
            printf("\n");

            printf("    void %s::init(struct ::wl_registry *registry, int id, int version)\n",
                   interfaceName);
            printf("    {\n");
            printf("        m_%s = static_cast<struct ::%s *>(wlRegistryBind(registry, id, "
                   "&%s_interface, version));\n",
                   interfaceName,
                   interfaceName,
                   interfaceName);
            if (hasEvents)
                printf("        init_listener();\n");
            printf("    }\n");
            printf("\n");

            printf("    void %s::init(struct ::%s *obj)\n", interfaceName, interfaceName);
            printf("    {\n");
            printf("        m_%s = obj;\n", interfaceName);
            if (hasEvents)
                printf("        init_listener();\n");
            printf("    }\n");
            printf("\n");

            printf("    %s *%s::fromObject(struct ::%s *object)\n",
                   interfaceName,
                   interfaceName,
                   interfaceName);
            printf("    {\n");
            if (hasEvents) {
                printf("        if (wl_proxy_get_listener((struct ::wl_proxy *)object) != (void "
                       "*)&m_%s_listener)\n",
                       interfaceName);
                printf("            return nullptr;\n");
            }
            printf("        return static_cast<%s *>(%s_get_user_data(object));\n",
                   interfaceName,
                   interfaceName);
            printf("    }\n");
            printf("\n");

            printf("    bool %s::isInitialized() const\n", interfaceName);
            printf("    {\n");
            printf("        return m_%s != nullptr;\n", interfaceName);
            printf("    }\n");
            printf("\n");

            printf("    const struct wl_interface *%s::interface()\n", interfaceName);
            printf("    {\n");
            printf("        return &::%s_interface;\n", interfaceName);
            printf("    }\n");

            for (const WaylandEvent &e : interface.requests) {
                printf("\n");
                const WaylandArgument *new_id = newIdArgument(e.arguments);
                QByteArray new_id_str = "void ";
                if (new_id) {
                    if (new_id->interface.isEmpty())
                        new_id_str = "void *";
                    else
                        new_id_str = "struct ::" + new_id->interface + " *";
                }
                printf("    %s%s::", new_id_str.constData(), interfaceName);
                printEvent(e);
                printf("\n");
                printf("    {\n");
                for (const WaylandArgument &a : e.arguments) {
                    if (a.type != "array")
                        continue;
                    QByteArray array = a.name + "_data";
                    const char *arrayName = array.constData();
                    const char *variableName = a.name.constData();
                    printf("        struct wl_array %s;\n", arrayName);
                    printf("        %s.size = %s.size();\n", arrayName, variableName);
                    printf("        %s.data = static_cast<void *>(const_cast<char "
                           "*>(%s.constData()));\n",
                           arrayName,
                           variableName);
                    printf("        %s.alloc = 0;\n", arrayName);
                    printf("\n");
                }
                int actualArgumentCount =
                    new_id ? int(e.arguments.size()) - 1 : int(e.arguments.size());
                printf("        %s%s_%s(\n",
                       new_id ? "return " : "",
                       interfaceName,
                       e.name.constData());
                printf("            m_%s%s", interfaceName, actualArgumentCount > 0 ? "," : "");
                bool needsComma = false;
                for (const WaylandArgument &a : e.arguments) {
                    bool isNewId = a.type == "new_id";
                    if (isNewId && !a.interface.isEmpty())
                        continue;
                    if (needsComma)
                        printf(",");
                    needsComma = true;
                    printf("\n");
                    if (isNewId) {
                        printf("            interface,\n");
                        printf("            version");
                    } else {
                        QByteArray cType = waylandToCType(a.type, a.interface);
                        QByteArray qtType = waylandToQtType(a.type, a.interface, e.request);
                        if (a.type == "string")
                            printf("            %s.toUtf8().constData()", a.name.constData());
                        else if (a.type == "array")
                            printf("            &%s_data", a.name.constData());
                        else if (cType == qtType)
                            printf("            %s", a.name.constData());
                    }
                }
                printf(");\n");
                if (e.type == "destructor")
                    printf("        m_%s = nullptr;\n", interfaceName);
                printf("    }\n");
            }

            if (hasEvents) {
                printf("\n");
                for (const WaylandEvent &e : interface.events) {
                    printf("    void %s::", interfaceName);
                    printEvent(e, true);
                    printf("\n");
                    printf("    {\n");
                    printf("    }\n");
                    printf("\n");
                    printf("    void %s::", interfaceName);
                    printEventHandlerSignature(e, interfaceName, false);
                    printf("\n");
                    printf("    {\n");
                    printf("        Q_UNUSED(object);\n");
                    printf("        static_cast<%s *>(data)->%s(",
                           interfaceName,
                           e.name.constData());
                    bool needsComma = false;
                    for (const WaylandArgument &a : e.arguments) {
                        if (needsComma)
                            printf(",");
                        needsComma = true;
                        printf("\n");
                        const char *argumentName = a.name.constData();
                        if (a.type == "string")
                            printf("            QString::fromUtf8(%s)", argumentName);
                        else
                            printf("            %s", argumentName);
                    }
                    printf(");\n");

                    printf("    }\n");
                    printf("\n");
                }
                printf("    const struct %s_listener %s::m_%s_listener = {\n",
                       interfaceName,
                       interfaceName,
                       interfaceName);
                for (const WaylandEvent &e : interface.events) {
                    printf("        %s::handle_%s,\n", interfaceName, e.name.constData());
                }
                printf("    };\n");
                printf("\n");

                printf("    void %s::init_listener()\n", interfaceName);
                printf("    {\n");
                printf("        %s_add_listener(m_%s, &m_%s_listener, this);\n",
                       interfaceName,
                       interfaceName,
                       interfaceName);
                printf("    }\n");
            }
        }
        printf("}\n");
        printf("\n");
        printf("QT_WARNING_POP\n");
        printf("QT_END_NAMESPACE\n");
    }

    return true;
}

void Scanner::printErrors()
{
    if (m_xml->hasError())
        fprintf(stderr,
                "XML error: %s\nLine %lld, column %lld\n",
                m_xml->errorString().toLocal8Bit().constData(),
                m_xml->lineNumber(),
                m_xml->columnNumber());
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    Scanner scanner;

    if (!scanner.parseArguments(argc, argv)) {
        scanner.printUsage();
        return EXIT_FAILURE;
    }

    if (!scanner.process()) {
        scanner.printErrors();
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
