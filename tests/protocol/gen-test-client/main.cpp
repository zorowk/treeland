// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
// gen-test-client v3: template-based code generation. No fprintf escaping.
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
#include <map>
#include <sstream>
#include <fstream>

struct Argument { std::string name, type, interfaceName; bool allowNull = false; };
struct Event { std::string name; std::vector<Argument> arguments; };
struct Interface { std::string name; int version = 1; std::vector<Event> requests, events; };

static std::string xmlAttr(const std::string &line, const char *attr) {
    std::string key = std::string(attr) + "=\"";
    auto pos = line.find(key); if (pos == std::string::npos) return "";
    pos += key.size(); auto end = line.find("\"", pos);
    return end == std::string::npos ? "" : line.substr(pos, end - pos);
}

static std::vector<Interface> parseProtocol(const char *path) {
    std::vector<Interface> r;
    char buf[65536]; FILE *f = fopen(path, "r"); if (!f) return r;
    size_t len = fread(buf, 1, sizeof(buf)-1, f); fclose(f); if (!len) return r;
    buf[len] = 0; std::string text(buf);
    Interface curIf; bool inReq = false, inEv = false; Event cur; size_t pos = 0;
    while (pos < text.size()) {
        size_t eol = text.find('\n', pos); if (eol == std::string::npos) eol = text.size();
        std::string line = text.substr(pos, eol - pos); pos = eol + 1;
        while (!line.empty() && (line[0]==' '||line[0]=='\t'||line[0]=='\r')) line=line.substr(1);
        if (line.empty()||line.find("<!--")!=std::string::npos) continue;
        bool sc = line.find("/>") != std::string::npos;
        if (line.find("<interface")!=std::string::npos) {
            if(!curIf.name.empty()) r.push_back(curIf); curIf=Interface{};
            curIf.name=xmlAttr(line,"name"); curIf.version=atoi(xmlAttr(line,"version").c_str());
        } else if(line.find("</interface>")!=std::string::npos) {
            if(!curIf.name.empty()) r.push_back(curIf); curIf=Interface{};
        } else if(line.find("<request")!=std::string::npos) {
            cur=Event{}; cur.name=xmlAttr(line,"name");
            if(xmlAttr(line,"type")=="destructor") cur.name="__destructor";
            if(sc){if(cur.name!="__destructor")curIf.requests.push_back(cur);}
            else{inReq=true;inEv=false;}
        } else if(line.find("<event")!=std::string::npos) {
            cur=Event{}; cur.name=xmlAttr(line,"name");
            if(sc)curIf.events.push_back(cur); else{inReq=false;inEv=true;}
        } else if(line.find("</request>")!=std::string::npos||line.find("</event>")!=std::string::npos) {
            if(cur.name!="__destructor"){if(inReq)curIf.requests.push_back(cur);else curIf.events.push_back(cur);}
            inReq=inEv=false;
        } else if(line.find("<arg")!=std::string::npos&&(inReq||inEv)) {
            Argument a; a.name=xmlAttr(line,"name");a.type=xmlAttr(line,"type");
            a.interfaceName=xmlAttr(line,"interface"); a.allowNull=xmlAttr(line,"allow-null")=="true";
            cur.arguments.push_back(a);
        }
    }
    if(!curIf.name.empty()) r.push_back(curIf); return r;
}

static std::string cType(const Argument &a) {
    if(a.type=="uint"||a.type=="enum")return"uint32_t"; if(a.type=="int")return"int32_t";
    if(a.type=="string")return"const char *"; if(a.type=="fixed")return"wl_fixed_t";
    if(a.type=="array")return"struct wl_array *"; if(a.type=="fd")return"int32_t";
    if(a.type=="new_id"||a.type=="object")
        return a.interfaceName.empty()?"struct wl_proxy *":"struct "+a.interfaceName+" *";
    return"void *";
}

static std::string pf(const Argument &a) {
    if(a.type=="uint"||a.type=="enum")return"%u"; if(a.type=="int"||a.type=="fixed"||a.type=="fd")return"%d";
    if(a.type=="array")return"%zu"; if(a.type=="string")return"%s"; return"?";
}

static std::string pv(const Argument &a) {
    if(a.type=="string")return a.name+" ? "+a.name+" : \"(null)\"";
    if(a.type=="array")return a.name+"->size"; if(a.type=="fd")return a.name+">=0 ? \"ok\" : \"invalid\"";
    return a.name;
}

// Load template, replace {{PLACEHOLDER}} with value
static std::string replace(const std::string &tmpl, const std::string &key, const std::string &value) {
    std::string r = tmpl;
    std::string placeholder = "{{" + key + "}}";
    size_t pos = 0;
    while ((pos = r.find(placeholder, pos)) != std::string::npos) {
        r.replace(pos, placeholder.size(), value);
        pos += value.size();
    }
    return r;
}

// Generate a block by joining lines with newlines
static std::string block(const std::vector<std::string> &lines, const std::string &indent = "") {
    std::string r;
    for (const auto &l : lines) r += indent + l + "\n";
    return r;
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "Usage: %s <protocol.xml> <output.c>\n", argv[0]); return 1; }
    auto ifaces = parseProtocol(argv[1]);
    if (ifaces.empty()) { fprintf(stderr, "Parse failed\n"); return 1; }

    // Use first interface as parent — it is always the global (manager).
    // Child interfaces with events are created via new_id, not bound directly.
    const Interface &parent = ifaces[0];

    // Find children referenced by new_id
    std::map<std::string, const Interface *> children;
    for (const Event &ev : parent.events)
        for (const Argument &a : ev.arguments)
            if (a.type == "new_id" && !a.interfaceName.empty())
                for (const Interface &ci : ifaces) if (ci.name == a.interfaceName) children[a.interfaceName] = &ci;
    for (const Event &ev : parent.requests)
        for (const Argument &a : ev.arguments)
            if (a.type == "new_id" && !a.interfaceName.empty())
                for (const Interface &ci : ifaces) if (ci.name == a.interfaceName) children[a.interfaceName] = &ci;

    // Derive basename
    std::string path(argv[1]); auto s = path.rfind('/');
    std::string fn = (s != std::string::npos) ? path.substr(s+1) : path;
    auto d = fn.rfind(".xml"); std::string base = (d != std::string::npos) ? fn.substr(0,d) : fn;
    for (auto &c : base) if (c == '_') c = '-';

    // Load template
    // Find template relative to executable or source
    std::string tmplPath = std::string(argv[0]);
    auto slash = tmplPath.rfind('/');
    if (slash != std::string::npos) tmplPath = tmplPath.substr(0, slash+1) + "template.c.in";

    std::ifstream tf(tmplPath);
    if (!tf.is_open()) {
        tf.open("template.c.in");
    }
    if (!tf.is_open()) {
        fprintf(stderr, "Cannot find template.c.in\n");
        return 1;
    }
    std::stringstream ss; ss << tf.rdbuf();
    std::string tmpl = ss.str();

    // ---- Build replacements ----

    // INCLUDES
    std::string includes = "#include \"wayland-" + base + "-client-protocol.h\"\n";
    includes += "#include <stdio.h>\n#include <stdlib.h>\n#include <string.h>\n";
    includes += "#include <unistd.h>\n#include <wayland-client.h>\n";
    tmpl = replace(tmpl, "INCLUDES", includes);

    // PROTOCOL_NAME
    tmpl = replace(tmpl, "PROTOCOL_NAME", fn);

    // PARENT_INTERFACE_NAME
    tmpl = replace(tmpl, "PARENT_INTERFACE_NAME", parent.name);

    // PARENT_DESTROY
    tmpl = replace(tmpl, "PARENT_DESTROY", parent.name + "_destroy");

    // Fixture detection: check if protocol needs surface/seat/output fixtures
    bool needSurface = false;
    for (const Event &ev : parent.requests)
        for (const Argument &a : ev.arguments) {
            if (a.type == "object" && a.interfaceName == "xdg_toplevel") needSurface = true;
            if (a.type == "object" && a.interfaceName == "wl_surface") needSurface = true;
        }
    
    std::string fixtureDefines;
    if (needSurface) fixtureDefines += "#define NEED_SURFACE_FIXTURE\n";
    tmpl = replace(tmpl, "FIXTURE_DEFINES", fixtureDefines);

    // CHILD_GLOBALS
    std::vector<std::string> cg;
    for (auto &kv : children)
        cg.push_back("static struct " + kv.first + " *g_child_" + kv.second->name + " = NULL;");
    tmpl = replace(tmpl, "CHILD_GLOBALS", block(cg, ""));

    // CHILD_EVENT_HANDLERS + CHILD_LISTENERS
    std::vector<std::string> ceh, cl;
    for (auto &kv : children) {
        const Interface &ci = *kv.second;
        if (ci.events.empty()) continue;
        for (const Event &ev : ci.events) {
            std::string sig = "static void handle_" + ci.name + "_" + ev.name + "(void *d, struct " + ci.name + " *p";
            for (const Argument &a : ev.arguments) sig += ", " + cType(a) + " " + a.name;
            sig += ")\n{\n    (void)d; (void)p;\n";
            sig += "    printf(\"EVENT " + ci.name + "." + ev.name;
            for (const Argument &a : ev.arguments) sig += " " + a.name + "=" + pf(a);
            sig += "\\n\"";
            for (const Argument &a : ev.arguments) sig += ", " + pv(a);
            sig += ");\n    fflush(stdout);\n}\n";
            ceh.push_back(sig);
        }
        std::string ls = "static const struct " + ci.name + "_listener child_" + ci.name + "_listener = {\n";
        for (const Event &ev : ci.events)
            ls += "    ." + ev.name + " = handle_" + ci.name + "_" + ev.name + ",\n";
        ls += "};";
        cl.push_back(ls);
    }
    tmpl = replace(tmpl, "CHILD_EVENT_HANDLERS", block(ceh, ""));
    tmpl = replace(tmpl, "CHILD_LISTENERS", block(cl, ""));

    // PARENT_EVENT_HANDLERS
    std::vector<std::string> peh;
    for (const Event &ev : parent.events) {
        bool hasSpecial = false;
        for (const Argument &a : ev.arguments)
            if (a.type == "new_id" || a.type == "object") hasSpecial = true;

        if (hasSpecial) {
            std::string sig = "static void handle_" + ev.name + "(void *d, struct " + parent.name + " *p";
            for (const Argument &a : ev.arguments) sig += ", " + cType(a) + " " + a.name;
            sig += ")\n{\n    (void)d; (void)p;\n";
            for (const Argument &a : ev.arguments) {
                if (a.type == "new_id" && children.count(a.interfaceName)) {
                    const Interface *ci = children[a.interfaceName];
                    if (!ci->events.empty())
                        sig += "    if (" + a.name + ") { g_child_" + a.interfaceName + " = " + a.name + "; "
                               + a.interfaceName + "_add_listener(" + a.name + ", &child_" + a.interfaceName + "_listener, NULL); }\n";
                    else
                        sig += "    if (" + a.name + ") { g_child_" + a.interfaceName + " = " + a.name + "; }\n";
                } else if (a.type == "object") {
                    sig += "    printf(\"EVENT " + parent.name + "." + ev.name + " object=%s\\n\", " + a.name + " ? \"present\" : \"null\");\n";
                    sig += "    fflush(stdout);\n";
                }
            }
            sig += "}\n";
            peh.push_back(sig);
        } else {
            std::string sig = "static void handle_" + ev.name + "(void *d, struct " + parent.name + " *p";
            for (const Argument &a : ev.arguments) sig += ", " + cType(a) + " " + a.name;
            sig += ")\n{\n    (void)d; (void)p;\n";
            sig += "    printf(\"EVENT " + ev.name;
            for (const Argument &a : ev.arguments) sig += " " + a.name + "=" + pf(a);
            sig += "\\n\"";
            for (const Argument &a : ev.arguments) sig += ", " + pv(a);
            sig += ");\n    fflush(stdout);\n}\n";
            peh.push_back(sig);
        }
    }
    tmpl = replace(tmpl, "PARENT_EVENT_HANDLERS", block(peh, ""));

    // PARENT_LISTENER
    if (!parent.events.empty()) {
        std::string pl = "static const struct " + parent.name + "_listener test_listener = {\n";
        for (const Event &ev : parent.events)
            pl += "    ." + ev.name + " = handle_" + ev.name + ",\n";
        pl += "};";
        tmpl = replace(tmpl, "PARENT_LISTENER", pl);
    } else {
        tmpl = replace(tmpl, "PARENT_LISTENER", "// no events");
    }

    // PARENT_ADD_LISTENER
    if (!parent.events.empty())
        tmpl = replace(tmpl, "PARENT_ADD_LISTENER", "        if (*pp) " + parent.name + "_add_listener(*pp, &test_listener, NULL);");
    else
        tmpl = replace(tmpl, "PARENT_ADD_LISTENER", "");

    // REQUEST_DISPATCH
    std::vector<std::string> rd;
    for (const Event &req : parent.requests) {
        if (req.name == "__destructor") continue;
        int strArgs = 0; for (const Argument &a : req.arguments) if (a.type != "new_id" && a.type != "object") strArgs++;
        std::string r = "    if (strcmp(name, \"" + req.name + "\") == 0) {\n";
        r += "        if (argc < " + std::to_string(strArgs) + ") { fprintf(stderr, \"%s expects " + std::to_string(strArgs) + " args\\n\", name); return -1; }\n";
        int si = 0;
        for (size_t i = 0; i < req.arguments.size(); i++) {
            const Argument &a = req.arguments[i];
            if (a.type == "new_id") continue;
            if (a.type == "object") {
                if (a.interfaceName == "xdg_toplevel") {
#ifdef NEED_SURFACE_FIXTURE
                    r += "        struct xdg_toplevel *a_" + a.name + " = g_xdg_toplevel;\n";
#else
                    r += "        void *a_" + a.name + " = NULL;\n";
#endif
                } else if (a.interfaceName == "wl_surface")
                    r += "        if (!g_surface && g_compositor) g_surface = wl_compositor_create_surface(g_compositor);\n        struct wl_surface *a_" + a.name + " = g_surface;\n";
                else if (a.interfaceName == "wl_output")
                    r += "        struct wl_output *a_" + a.name + " = g_output;\n";
                else if (a.interfaceName == "wl_seat")
                    r += "        struct wl_seat *a_" + a.name + " = g_seat;\n";
                else if (a.interfaceName == "wl_buffer")
                    r += "        void *a_" + a.name + " = NULL;\n";
                else
                    r += "        void *a_" + a.name + " = NULL;\n";
                continue;
            }
            if (a.type == "uint" || a.type == "enum")
                r += "        uint32_t a_" + a.name + " = (uint32_t)strtoul(argv[" + std::to_string(si) + "], NULL, 10);\n";
            else if (a.type == "int")
                r += "        int32_t a_" + a.name + " = (int32_t)strtol(argv[" + std::to_string(si) + "], NULL, 10);\n";
            else if (a.type == "fixed")
                r += "        wl_fixed_t a_" + a.name + " = wl_fixed_from_double(strtod(argv[" + std::to_string(si) + "], NULL));\n";
            else if (a.type == "string")
                r += "        const char *a_" + a.name + " = (argv[" + std::to_string(si) + "][0] == '-' && argv[" + std::to_string(si) + "][1] == 0) ? NULL : argv[" + std::to_string(si) + "];\n";
            else if (a.type == "array") {
                r += "        struct wl_array a_" + a.name + " = {0};\n";
                r += "        if (strncmp(argv[" + std::to_string(si) + "], \"hex:\", 4) == 0) {\n";
                r += "            const char *h = argv[" + std::to_string(si) + "] + 4;\n";
                r += "            size_t l = strlen(h) / 2;\n";
                r += "            uint8_t *d = malloc(l);\n";
                r += "            for (size_t j = 0; j < l; j++) { char b[3] = {h[j*2], h[j*2+1], 0}; d[j] = (uint8_t)strtoul(b, NULL, 16); }\n";
                r += "            a_" + a.name + ".data = d; a_" + a.name + ".size = l;\n        }\n";
            } else if (a.type == "fd") {
                r += "        int a_" + a.name + " = -1;\n";
                r += "        if (strcmp(argv[" + std::to_string(si) + "], \"fd:pipe\") == 0) { int pp[2]; if (pipe(pp) == 0) { a_" + a.name + " = pp[0]; close(pp[1]); } }\n";
            }
            si++;
        }
        // Call
        bool hasNewId = false;
        for (const Argument &a : req.arguments) if (a.type == "new_id") hasNewId = true;
        if (hasNewId)
            for (const Argument &a : req.arguments) if (a.type == "new_id")
                r += "        struct " + a.interfaceName + " *a_" + a.name + " = ";
        r += parent.name + "_" + req.name + "(p";
        for (const Argument &a : req.arguments) {
            if (a.type == "new_id") continue;
            r += ", " + std::string(a.type == "array" ? "&" : "") + "a_" + a.name;
        }
        r += ");\n";
        // Post-call
        for (const Argument &a : req.arguments) {
            if (a.type == "array") r += "        if (a_" + a.name + ".data) free(a_" + a.name + ".data);\n";
            if (a.type == "fd") r += "        if (a_" + a.name + " >= 0) close(a_" + a.name + ");\n";
            if (a.type == "new_id" && children.count(a.interfaceName)) {
                const Interface *ci = children[a.interfaceName];
                if (!ci->events.empty())
                    r += "        if (a_" + a.name + ") { g_child_" + a.interfaceName + " = a_" + a.name + "; "
                         + a.interfaceName + "_add_listener(a_" + a.name + ", &child_" + a.interfaceName + "_listener, NULL); } else return -1;\n";
                else
                    r += "        if (a_" + a.name + ") { g_child_" + a.interfaceName + " = a_" + a.name + "; } else return -1;\n";
            } else if (a.type == "new_id")
                r += "        if (!a_" + a.name + ") return -1;\n";
        }
        r += "        return " + std::to_string(si) + ";\n    }\n";
        rd.push_back(r);
    }
    tmpl = replace(tmpl, "REQUEST_DISPATCH", block(rd, ""));

    // CLEANUP
    std::vector<std::string> clean;
    clean.push_back("    if (g_surface) wl_surface_destroy(g_surface);");
    clean.push_back("    if (g_compositor) wl_compositor_destroy(g_compositor);");
    clean.push_back("    if (g_output) wl_output_destroy(g_output);");
    clean.push_back("    if (g_seat) wl_seat_destroy(g_seat);");
    clean.push_back("#ifdef NEED_SURFACE_FIXTURE");
    clean.push_back("    if (g_xdg_toplevel) xdg_toplevel_destroy(g_xdg_toplevel);");
    clean.push_back("    if (g_xdg_surface) xdg_surface_destroy(g_xdg_surface);");
    clean.push_back("    if (g_xdg_wm_base) xdg_wm_base_destroy(g_xdg_wm_base);");
    clean.push_back("#endif");
    for (auto &kv : children) {
        clean.push_back("    if (g_child_" + kv.second->name + ") " + kv.first + "_destroy(g_child_" + kv.second->name + ");");
    }
    tmpl = replace(tmpl, "CLEANUP", block(clean, "    "));

    // Write output
    FILE *out = fopen(argv[2], "w");
    if (!out) { fprintf(stderr, "Cannot open %s\n", argv[2]); return 1; }
    fprintf(out, "%s", tmpl.c_str());
    fclose(out);
    printf("Generated %s\n", argv[2]);
    return 0;
}
