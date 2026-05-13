#include "legacy/legacy_main.h"
#include "experimental/lidl_gen_client.h"
#include "experimental/lidl_gen_provider.h"
#include "experimental/lidl_serializer.h"
#include "experimental/impl_header_parser.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>

int main(int argc, char* argv[])
{
    // Check for --lidl or --from-header mode before initializing QCoreApplication,
    // since legacy_main creates its own.
    bool hasLidl = false;
    bool hasFromHeader = false;
    for (int i = 1; i < argc; ++i) {
        QString arg = QString::fromUtf8(argv[i]);
        if (arg == "--lidl") hasLidl = true;
        if (arg == "--from-header") hasFromHeader = true;
    }

    if (hasLidl || hasFromHeader) {
        QCoreApplication app(argc, argv);
        QTextStream err(stderr);
        QTextStream out(stdout);
        const QStringList args = app.arguments();

        QString outputDir;
        const int outDirIdx = args.indexOf("--output-dir");
        if (outDirIdx != -1 && outDirIdx + 1 < args.size()) {
            outputDir = args.at(outDirIdx + 1);
        }

        // --from-header mode: parse C++ impl header directly (no .lidl needed)
        if (hasFromHeader) {
            const int fromHeaderIdx = args.indexOf("--from-header");
            if (fromHeaderIdx + 1 >= args.size()) {
                err << "Error: --from-header requires a path to the impl header\n";
                return 1;
            }
            QString headerPath = args.at(fromHeaderIdx + 1);

            const int implClassIdx = args.indexOf("--impl-class");
            if (implClassIdx == -1 || implClassIdx + 1 >= args.size()) {
                err << "Error: --from-header requires --impl-class <ClassName>\n";
                return 1;
            }
            QString implClass = args.at(implClassIdx + 1);

            const int metadataIdx = args.indexOf("--metadata");
            if (metadataIdx == -1 || metadataIdx + 1 >= args.size()) {
                err << "Error: --from-header requires --metadata <metadata.json>\n";
                return 1;
            }
            QString metadataPath = args.at(metadataIdx + 1);

            // --impl-header: the include path for generated code (defaults to header filename)
            QString implHeader;
            const int implHeaderIdx = args.indexOf("--impl-header");
            if (implHeaderIdx != -1 && implHeaderIdx + 1 < args.size()) {
                implHeader = args.at(implHeaderIdx + 1);
            } else {
                implHeader = QFileInfo(headerPath).fileName();
            }

            const int backendIdx = args.indexOf("--backend");
            if (backendIdx == -1 || backendIdx + 1 >= args.size()) {
                err << "Error: --from-header requires --backend <qt>\n";
                return 1;
            }
            QString backend = args.at(backendIdx + 1);

            // Parse the impl header
            ImplParseResult pr = parseImplHeader(headerPath, implClass, metadataPath, err);
            if (pr.hasError()) {
                err << "Error parsing impl header: " << pr.error << "\n";
                return 4;
            }

            const ModuleDecl& mod = pr.module;
            QString genDirPath = outputDir.isEmpty()
                ? QDir::current().filePath("generated")
                : outputDir;
            QDir().mkpath(genDirPath);

            if (backend == "qt") {
                // Generate provider header (Qt glue)
                QString glueHeaderAbs = QDir(genDirPath).filePath(mod.name + "_qt_glue.h");
                {
                    QFile f(glueHeaderAbs);
                    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
                        err << "Failed to write glue header: " << glueHeaderAbs << "\n";
                        return 6;
                    }
                    f.write(lidlMakeProviderHeader(mod, implClass, implHeader).toUtf8());
                }

                // Generate dispatch source
                QString dispatchAbs = QDir(genDirPath).filePath(mod.name + "_dispatch.cpp");
                {
                    QFile f(dispatchAbs);
                    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
                        err << "Failed to write dispatch source: " << dispatchAbs << "\n";
                        return 7;
                    }
                    f.write(lidlMakeProviderDispatch(mod).toUtf8());
                }

                out << "Generated: " << glueHeaderAbs << "\n";
                out << "Generated: " << dispatchAbs << "\n";

                // Events bodies (Qt-MOC-style) and LIDL sidecar — emitted
                // when the impl declares any `logos_events:` prototypes.
                // The sidecar travels in the dep's headers-* output so
                // consumer-side codegen can generate typed `on<X>()`
                // accessors without reintrospecting the .dylib.
                if (!mod.events.isEmpty()) {
                    QString eventsAbs = QDir(genDirPath).filePath(mod.name + "_events.cpp");
                    {
                        QFile f(eventsAbs);
                        if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
                            err << "Failed to write events source: " << eventsAbs << "\n";
                            return 8;
                        }
                        f.write(lidlMakeEventsSource(mod, implClass, implHeader).toUtf8());
                    }
                    out << "Generated: " << eventsAbs << "\n";

                    QString lidlAbs = QDir(genDirPath).filePath(mod.name + ".lidl");
                    {
                        QFile f(lidlAbs);
                        if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
                            err << "Failed to write LIDL sidecar: " << lidlAbs << "\n";
                            return 9;
                        }
                        f.write(lidlSerialize(mod).toUtf8());
                    }
                    out << "Generated: " << lidlAbs << "\n";
                }

                out.flush();
                return 0;
            }

            err << "Error: --from-header currently only supports --backend qt\n";
            return 1;
        }

        // --lidl mode
        const int lidlIdx = args.indexOf("--lidl");
        if (lidlIdx + 1 >= args.size()) {
            err << "Usage: " << QFileInfo(app.applicationFilePath()).fileName()
                << " --lidl /path/to/module.lidl [--output-dir /path] [--module-only]\n"
                << "       " << QFileInfo(app.applicationFilePath()).fileName()
                << " --lidl /path/to/module.lidl --backend qt --impl-class Class --impl-header header.h [--output-dir /path]\n"
                << "       " << QFileInfo(app.applicationFilePath()).fileName()
                << " --from-header src/impl.h --backend qt --impl-class Class --metadata metadata.json [--output-dir /path]\n";
            return 1;
        }
        QString lidlPath = args.at(lidlIdx + 1);

        // Provider glue mode: --backend qt --impl-class X --impl-header Y
        const int backendIdx = args.indexOf("--backend");
        if (backendIdx != -1) {
            if (backendIdx + 1 >= args.size()) {
                err << "Error: --backend requires an argument (e.g., qt)\n";
                return 1;
            }
            QString backend = args.at(backendIdx + 1);

            const int implClassIdx = args.indexOf("--impl-class");
            const int implHeaderIdx = args.indexOf("--impl-header");
            if (implClassIdx == -1 || implClassIdx + 1 >= args.size()) {
                err << "Error: --backend " << backend << " requires --impl-class <ClassName>\n";
                return 1;
            }
            if (implHeaderIdx == -1 || implHeaderIdx + 1 >= args.size()) {
                err << "Error: --backend " << backend << " requires --impl-header <header.h>\n";
                return 1;
            }

            QString implClass = args.at(implClassIdx + 1);
            QString implHeader = args.at(implHeaderIdx + 1);

            if (backend == "qt")
                return lidlGenerateProviderGlue(lidlPath, implClass, implHeader, outputDir, out, err);

            err << "Error: unsupported backend '" << backend << "' (supported: qt)\n";
            return 1;
        }

        // Client stub mode (default)
        bool moduleOnly = args.contains("--module-only");
        return lidlGenerateClientStubs(lidlPath, outputDir, moduleOnly, out, err);
    }

    return legacy_main(argc, argv);
}
