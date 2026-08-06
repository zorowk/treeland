// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
// gen-test-client: generate standalone C test client from Wayland protocol XML
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
#include <map>

struct Argument { std::string name, type, interfaceName; bool allowNull = false; };
struct Event { std::string name; int since = 1; std::vector<Argument> arguments; };
struct Interface { std::string name; int version = 1; std::vector<Event> requests, events; };

static std::string xmlAttr(const std::string &line, const char *attr) {
    std::string key = std::string(attr) + "=\"";
    auto pos = line.find(key); if (pos == std::string::npos) return "";
    pos += key.size(); auto end = line.find("\"", pos);
    return end == std::string::npos ? "" : line.substr(pos, end - pos);
}

// Robust parser: handles self-closing tags (/>)
static std::vector<Interface> parseProtocol(const char *path) {
    std::vector<Interface> r;
    char buf[65536];
    FILE *f = fopen(path, "r"); if (!f) return r;
    size_t len = fread(buf, 1, sizeof(buf)-1, f); fclose(f);
    if (len == 0) return r;
    buf[len] = 0;
    std::string text(buf);

    Interface curIf;
    bool inReq = false, inEv = false; Event cur;
    size_t pos = 0;

    while (pos < text.size()) {
        size_t eol = text.find('\n', pos);
        if (eol == std::string::npos) eol = text.size();
        std::string line = text.substr(pos, eol - pos);
        pos = eol + 1;

        // Trim whitespace
        while (!line.empty() && (line[0] == ' ' || line[0] == '\t' || line[0] == '\r'))
            line = line.substr(1);

        if (line.empty() || line.find("<!--") != std::string::npos) continue;
        bool selfClose = line.find("/>") != std::string::npos;

        if (line.find("<interface") != std::string::npos) {
            if (!curIf.name.empty()) r.push_back(curIf);
            curIf = Interface{};
            curIf.name = xmlAttr(line, "name");
            curIf.version = atoi(xmlAttr(line, "version").c_str());
        } else if (line.find("</interface>") != std::string::npos) {
            if (!curIf.name.empty()) r.push_back(curIf);
            curIf = Interface{};
        } else if (line.find("<request") != std::string::npos) {
            cur = Event{};
            cur.name = xmlAttr(line, "name");
            if (xmlAttr(line, "type") == "destructor") cur.name = "__destructor";
            if (selfClose) {
                if (cur.name != "__destructor") curIf.requests.push_back(cur);
            } else {
                inReq = true; inEv = false;
            }
        } else if (line.find("<event") != std::string::npos) {
            cur = Event{}; cur.name = xmlAttr(line, "name");
            if (selfClose) {
                curIf.events.push_back(cur);
            } else {
                inReq = false; inEv = true;
            }
        } else if (line.find("</request>") != std::string::npos || line.find("</event>") != std::string::npos) {
            if (cur.name != "__destructor") {
                if (inReq) curIf.requests.push_back(cur); else curIf.events.push_back(cur);
            }
            inReq = inEv = false;
        } else if (line.find("<arg") != std::string::npos && (inReq || inEv)) {
            Argument a;
            a.name = xmlAttr(line, "name"); a.type = xmlAttr(line, "type");
            a.interfaceName = xmlAttr(line, "interface");
            a.allowNull = xmlAttr(line, "allow-null") == "true";
            cur.arguments.push_back(a);
        } else if (line.find("<enum") != std::string::npos || line.find("<entry") != std::string::npos
                   || line.find("<description") != std::string::npos) {
            // skip enums and descriptions
        }
    }
    if (!curIf.name.empty()) r.push_back(curIf);
    return r;
}

static std::string cType(const Argument &arg) {
    if (arg.type == "uint" || arg.type == "enum") return "uint32_t";
    if (arg.type == "int") return "int32_t";
    if (arg.type == "string") return "const char *";
    if (arg.type == "fixed") return "wl_fixed_t";
    if (arg.type == "array") return "struct wl_array *";
    if (arg.type == "fd") return "int32_t";
    if (arg.type == "new_id" || arg.type == "object")
        return arg.interfaceName.empty() ? "struct wl_proxy *" : "struct " + arg.interfaceName + " *";
    return "void *";
}
static std::string pf(const Argument &arg) {
    if (arg.type == "uint" || arg.type == "enum") return "%u";
    if (arg.type == "int" || arg.type == "fixed" || arg.type == "fd") return "%d";
    if (arg.type == "array") return "%zu";
    if (arg.type == "string") return "%s";
    return "?";
}
static std::string pv(const Argument &arg) {
    if (arg.type == "string") return arg.name + " ? " + arg.name + " : \"(null)\"";
    if (arg.type == "array") return arg.name + "->size";
    if (arg.type == "fd") return arg.name + ">=0 ? \"ok\" : \"invalid\"";
    return arg.name;
}

void generateClient(const std::vector<Interface> &ifaces, const std::string &basename, FILE *out) {
    if (ifaces.empty()) return;
    const Interface &parent = ifaces[0];
    std::map<std::string, const Interface *> children;
    for (const Event &ev : parent.events)
        for (const Argument &a : ev.arguments)
            if (a.type == "new_id" && !a.interfaceName.empty())
                for (const Interface &ci : ifaces) if (ci.name == a.interfaceName) children[a.interfaceName] = &ci;
    for (const Event &ev : parent.requests)
        for (const Argument &a : ev.arguments)
            if (a.type == "new_id" && !a.interfaceName.empty())
                for (const Interface &ci : ifaces) if (ci.name == a.interfaceName) children[a.interfaceName] = &ci;

    fprintf(out, "// Auto-generated by gen-test-client\n\n");
    fprintf(out, "#include \"wayland-%s-client-protocol.h\"\n", basename.c_str());
    fprintf(out, "#include <stdio.h>\n#include <stdlib.h>\n#include <string.h>\n");
    fprintf(out, "#include <unistd.h>\n#include <wayland-client.h>\n\n");

    for (auto &kv : children)
        fprintf(out, "static struct %s *g_child_%s = NULL;\n", kv.first.c_str(), kv.second->name.c_str());

    for (auto &kv : children) {
        const Interface &ci = *kv.second;
        for (const Event &ev : ci.events) {
            fprintf(out, "static void handle_%s_%s(void *d, struct %s *p", ci.name.c_str(), ev.name.c_str(), ci.name.c_str());
            for (const Argument &a : ev.arguments) fprintf(out, ", %s %s", cType(a).c_str(), a.name.c_str());
            fprintf(out, ")\n{\n    (void)d; (void)p;\n    printf(\"EVENT %s.%s", ci.name.c_str(), ev.name.c_str());
            for (const Argument &a : ev.arguments) fprintf(out, " %s=%s", a.name.c_str(), pf(a).c_str());
            fprintf(out, "\\n\"");
            for (const Argument &a : ev.arguments) fprintf(out, ", %s", pv(a).c_str());
            fprintf(out, ");\n    fflush(stdout);\n}\n\n");
        }
        fprintf(out, "static const struct %s_listener child_%s_listener = {\n", ci.name.c_str(), ci.name.c_str());
        for (const Event &ev : ci.events) fprintf(out, "    .%s = handle_%s_%s,\n", ev.name.c_str(), ci.name.c_str(), ev.name.c_str());
        fprintf(out, "};\n\n");
    }

    for (const Event &ev : parent.events) {
        fprintf(out, "static void handle_%s(void *d, struct %s *p", ev.name.c_str(), parent.name.c_str());
        for (const Argument &a : ev.arguments) fprintf(out, ", %s %s", cType(a).c_str(), a.name.c_str());
        fprintf(out, ")\n{\n    (void)d; (void)p;\n");
        bool hasSpecial = false;
        for (const Argument &a : ev.arguments) {
            if (a.type == "new_id" && children.count(a.interfaceName)) {
                hasSpecial = true;
                fprintf(out, "    if (%s) { g_child_%s=%s; %s_add_listener(%s,&child_%s_listener,NULL); }\n",
                       a.name.c_str(), a.interfaceName.c_str(), a.name.c_str(),
                       a.interfaceName.c_str(), a.name.c_str(), a.interfaceName.c_str());
            } else if (a.type == "object") {
                hasSpecial = true;
                fprintf(out, "    printf(\"EVENT %s.%s object=%%s\\n\", %s?\"present\":\"null\");\n",
                       parent.name.c_str(), ev.name.c_str(), a.name.c_str());
                fprintf(out, "    fflush(stdout);\n");
            }
        }
        if (!hasSpecial) {
            fprintf(out, "    printf(\"EVENT %s", ev.name.c_str());
            for (const Argument &a : ev.arguments) fprintf(out, " %s=%s", a.name.c_str(), pf(a).c_str());
            fprintf(out, "\\n\"");
            for (const Argument &a : ev.arguments) fprintf(out, ", %s", pv(a).c_str());
            fprintf(out, ");\n    fflush(stdout);\n");
        }
        fprintf(out, "}\n\n");
    }

    fprintf(out, "static const struct %s_listener test_listener = {\n", parent.name.c_str());
    for (const Event &ev : parent.events) fprintf(out, "    .%s = handle_%s,\n", ev.name.c_str(), ev.name.c_str());
    fprintf(out, "};\n\n");
    fprintf(out, "static void handle_global(void *d, struct wl_registry *r, uint32_t n, const char *i, uint32_t v)\n{\n");
    fprintf(out, "    if (strcmp(i,%s_interface.name)==0) {\n", parent.name.c_str());
    fprintf(out, "        struct %s **pp=(struct %s **)d;\n", parent.name.c_str(), parent.name.c_str());
    fprintf(out, "        *pp=(struct %s *)wl_registry_bind(r,n,&%s_interface,v);\n", parent.name.c_str(), parent.name.c_str());
    fprintf(out, "        if(*pp) %s_add_listener(*pp,&test_listener,NULL);\n    }\n}\n\n", parent.name.c_str());
    fprintf(out, "static void handle_global_remove(void *d, struct wl_registry *r, uint32_t n){(void)d;(void)r;(void)n;}\n\n");
    fprintf(out, "static const struct wl_registry_listener rlistener={.global=handle_global,.global_remove=handle_global_remove};\n\n");

    fprintf(out, "static int dispatch_request(struct %s *p,const char *name,int argc,char **argv)\n{\n", parent.name.c_str());
    for (const Event &req : parent.requests) {
        if (req.name == "__destructor") continue;
        int strArgs = 0; for (const Argument &a : req.arguments) if (a.type != "new_id") strArgs++;
        fprintf(out, "    if(strcmp(name,\"%s\")==0) {\n", req.name.c_str());
        fprintf(out, "        if(argc<%d){fprintf(stderr,\"%%s expects %d args\\n\",name);return -1;}\n", strArgs, strArgs);
        int si = 0;
        for (size_t i = 0; i < req.arguments.size(); i++) {
            const Argument &a = req.arguments[i];
            if (a.type == "new_id") continue;
            if (a.type == "uint" || a.type == "enum")
                fprintf(out, "        uint32_t a_%s=(uint32_t)strtoul(argv[%d],NULL,10);\n", a.name.c_str(), si);
            else if (a.type == "int")
                fprintf(out, "        int32_t a_%s=(int32_t)strtol(argv[%d],NULL,10);\n", a.name.c_str(), si);
            else if (a.type == "fixed")
                fprintf(out, "        wl_fixed_t a_%s=wl_fixed_from_double(strtod(argv[%d],NULL));\n", a.name.c_str(), si);
            else if (a.type == "string")
                fprintf(out, "        const char *a_%s=(argv[%d][0]=='-'&&argv[%d][1]==0)?NULL:argv[%d];\n", a.name.c_str(), si, si, si);
            else if (a.type == "array") {
                fprintf(out, "        struct wl_array a_%s={0}; if(strncmp(argv[%d],\"hex:\",4)==0){\n", a.name.c_str(), si);
                fprintf(out, "            const char *h=argv[%d]+4; size_t l=strlen(h)/2; uint8_t *d=malloc(l);\n", si);
                fprintf(out, "            for(size_t j=0;j<l;j++){char b[3]={h[j*2],h[j*2+1],0};d[j]=(uint8_t)strtoul(b,NULL,16);}\n");
                fprintf(out, "            a_%s.data=d;a_%s.size=l;}\n", a.name.c_str(), a.name.c_str());
            } else if (a.type == "fd") {
                fprintf(out, "        int a_%s=-1; if(strcmp(argv[%d],\"fd:pipe\")==0){int pp[2];if(pipe(pp)==0){a_%s=pp[0];close(pp[1]);}}\n",
                       a.name.c_str(), si, a.name.c_str());
            }
            si++;
        }
        bool hasNewId = false;
        for (const Argument &a : req.arguments) if (a.type == "new_id") hasNewId = true;
        if (hasNewId)
            for (const Argument &a : req.arguments) if (a.type == "new_id")
                    fprintf(out, "        struct %s *a_%s = ", a.interfaceName.c_str(), a.name.c_str());
        fprintf(out, "%s_%s(p", parent.name.c_str(), req.name.c_str());
        for (const Argument &a : req.arguments) {
            if (a.type == "new_id") continue;
            fprintf(out, ",%sa_%s", a.type == "array" ? "&" : "", a.name.c_str());
        }
        fprintf(out, ");\n");
        for (const Argument &a : req.arguments) {
            if (a.type == "new_id" && children.count(a.interfaceName)) {
                fprintf(out, "        if(a_%s){g_child_%s=a_%s;%s_add_listener(a_%s,&child_%s_listener,NULL);}else return -1;\n",
                       a.name.c_str(), a.interfaceName.c_str(), a.name.c_str(),
                       a.interfaceName.c_str(), a.name.c_str(), a.interfaceName.c_str());
            } else if (a.type == "new_id")
                fprintf(out, "        if(!a_%s) return -1;\n", a.name.c_str());
            if (a.type == "array") fprintf(out, "        if(a_%s.data)free(a_%s.data);\n", a.name.c_str(), a.name.c_str());
            if (a.type == "fd") fprintf(out, "        if(a_%s>=0)close(a_%s);\n", a.name.c_str(), a.name.c_str());
        }
        fprintf(out, "        return %d;\n    }\n", strArgs);
    }
    fprintf(out, "    fprintf(stderr,\"Unknown request: %%s\\n\",name);\n    return -1;\n}\n\n");

    fprintf(out, "int main(int argc,char**argv){\n");
    fprintf(out, "    const char *sock=NULL;\n");
    fprintf(out, "    for(int i=1;i<argc;i++)if(strcmp(argv[i],\"--socket\")==0&&i+1<argc)sock=argv[++i];\n");
    fprintf(out, "    if(!sock){fprintf(stderr,\"Usage: %%s --socket PATH [--request N ARGS...] [--roundtrip] [--checkpoint]\\n\",argv[0]);return 1;}\n");
    fprintf(out, "    struct wl_display *d=wl_display_connect(sock);if(!d)return 1;\n");
    fprintf(out, "    struct wl_registry *r=wl_display_get_registry(d);\n");
    fprintf(out, "    struct %s *p=NULL;\n", parent.name.c_str());
    fprintf(out, "    wl_registry_add_listener(r,&rlistener,&p);wl_display_roundtrip(d);\n");
    fprintf(out, "    if(!p){fprintf(stderr,\"Failed to bind %s\\n\");return 1;}\n", parent.name.c_str());
    fprintf(out, "    int rc=0;\n    for(int i=1;i<argc;i++){\n");
    fprintf(out, "        if(strcmp(argv[i],\"--socket\")==0){i++;continue;}\n");
    fprintf(out, "        if(strcmp(argv[i],\"--roundtrip\")==0){if(wl_display_roundtrip(d)<0){rc=1;break;}continue;}\n");
    fprintf(out, "        if(strcmp(argv[i],\"--checkpoint\")==0){printf(\"CHECKPOINT\\n\");fflush(stdout);continue;}\n");
    fprintf(out, "        if(strcmp(argv[i],\"--destroy\")==0){%s_destroy(p);p=NULL;continue;}\n", parent.name.c_str());
    fprintf(out, "        if(strcmp(argv[i],\"--request\")==0){\n");
    fprintf(out, "            if(i+1>=argc){rc=1;break;}\n");
    fprintf(out, "            const char *rn=argv[++i];\n");
    fprintf(out, "            int used=dispatch_request(p,rn,argc-i-1,&argv[i+1]);\n");
    fprintf(out, "            if(used<0){rc=1;break;}\n            i+=used;continue;\n        }\n    }\n");
    fprintf(out, "    if(p)%s_destroy(p);\n    wl_registry_destroy(r);\n    wl_display_disconnect(d);\n    return rc;\n}\n",
            parent.name.c_str());
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "Usage: %s <protocol.xml> <output.c>\n", argv[0]); return 1; }
    auto ifaces = parseProtocol(argv[1]);
    if (ifaces.empty()) { fprintf(stderr, "Parse failed\n"); return 1; }
    std::string path(argv[1]); auto s = path.rfind('/');
    std::string fn = (s != std::string::npos) ? path.substr(s+1) : path;
    auto d = fn.rfind(".xml"); std::string base = (d != std::string::npos) ? fn.substr(0,d) : fn;
    for (auto &c : base) if (c == '_') c = '-';
    FILE *out = fopen(argv[2], "w"); if (!out) { fprintf(stderr, "Cannot open %s\n", argv[2]); return 1; }
    generateClient(ifaces, base, out); fclose(out);
    printf("Generated %s\n", argv[2]); return 0;
}
