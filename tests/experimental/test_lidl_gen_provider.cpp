#include <gtest/gtest.h>
#include "lidl_gen_provider.h"
#include "lidl_gen_client.h"

static ModuleDecl makeTestModule()
{
    ModuleDecl m;
    m.name = "test_module";
    m.version = "1.0.0";
    m.description = "Test module";

    // Method: std::string greet(const std::string& name) → tstr
    {
        MethodDecl md;
        md.name = "greet";
        md.returnType = { TypeExpr::Primitive, "tstr", {} };
        ParamDecl p; p.name = "name"; p.type = { TypeExpr::Primitive, "tstr", {} };
        md.params.append(p);
        m.methods.append(md);
    }
    // Method: bool isValid() → bool
    {
        MethodDecl md;
        md.name = "isValid";
        md.returnType = { TypeExpr::Primitive, "bool", {} };
        m.methods.append(md);
    }
    // Method: int64_t getCount() → int
    {
        MethodDecl md;
        md.name = "getCount";
        md.returnType = { TypeExpr::Primitive, "int", {} };
        m.methods.append(md);
    }
    // Method: std::vector<std::string> getNames() → [tstr]
    {
        MethodDecl md;
        md.name = "getNames";
        TypeExpr elem = { TypeExpr::Primitive, "tstr", {} };
        md.returnType = { TypeExpr::Array, "", { elem } };
        m.methods.append(md);
    }
    // Method: void doNothing() → void
    {
        MethodDecl md;
        md.name = "doNothing";
        md.returnType = { TypeExpr::Primitive, "void", {} };
        m.methods.append(md);
    }
    return m;
}

// ---------------------------------------------------------------------------
// Provider header generation
// ---------------------------------------------------------------------------

TEST(LidlGenProvider, HeaderContainsClassName)
{
    auto m = makeTestModule();
    QString h = lidlMakeProviderHeader(m, "TestModuleImpl", "test_module_impl.h");
    EXPECT_TRUE(h.contains("TestModuleProviderObject"));
    EXPECT_TRUE(h.contains("TestModulePlugin"));
}

TEST(LidlGenProvider, HeaderIncludesImplHeader)
{
    auto m = makeTestModule();
    QString h = lidlMakeProviderHeader(m, "TestModuleImpl", "test_module_impl.h");
    EXPECT_TRUE(h.contains("#include \"test_module_impl.h\""));
}

TEST(LidlGenProvider, HeaderIncludesFrameworkHeaders)
{
    auto m = makeTestModule();
    QString h = lidlMakeProviderHeader(m, "TestModuleImpl", "test_module_impl.h");
    EXPECT_TRUE(h.contains("#include \"logos_provider_object.h\""));
    EXPECT_TRUE(h.contains("#include \"interface.h\""));
    EXPECT_TRUE(h.contains("#include \"logos_types.h\""));
}

TEST(LidlGenProvider, HeaderHasPluginMetadata)
{
    auto m = makeTestModule();
    QString h = lidlMakeProviderHeader(m, "TestModuleImpl", "test_module_impl.h");
    EXPECT_TRUE(h.contains("Q_PLUGIN_METADATA"));
    EXPECT_TRUE(h.contains("Q_INTERFACES(PluginInterface LogosProviderPlugin)"));
    EXPECT_TRUE(h.contains("Q_OBJECT"));
}

TEST(LidlGenProvider, HeaderHasLogosMacro)
{
    auto m = makeTestModule();
    QString h = lidlMakeProviderHeader(m, "TestModuleImpl", "test_module_impl.h");
    EXPECT_TRUE(h.contains("LOGOS_PROVIDER(TestModuleProviderObject, \"test_module\", \"1.0.0\")"));
}

TEST(LidlGenProvider, HeaderHasWrapperMethods)
{
    auto m = makeTestModule();
    QString h = lidlMakeProviderHeader(m, "TestModuleImpl", "test_module_impl.h");
    EXPECT_TRUE(h.contains("QString greet("));
    EXPECT_TRUE(h.contains("bool isValid("));
    EXPECT_TRUE(h.contains("int getCount("));
    EXPECT_TRUE(h.contains("QStringList getNames("));
    EXPECT_TRUE(h.contains("void doNothing("));
}

TEST(LidlGenProvider, HeaderConvertsStringParams)
{
    auto m = makeTestModule();
    QString h = lidlMakeProviderHeader(m, "TestModuleImpl", "test_module_impl.h");
    // greet should convert QString→std::string and back
    EXPECT_TRUE(h.contains(".toStdString()"));
    EXPECT_TRUE(h.contains("QString::fromStdString("));
}

TEST(LidlGenProvider, HeaderHasStringVecHelpers)
{
    auto m = makeTestModule();
    QString h = lidlMakeProviderHeader(m, "TestModuleImpl", "test_module_impl.h");
    // getNames returns [tstr], so helpers should be generated
    EXPECT_TRUE(h.contains("lidlToQStringList"));
    EXPECT_TRUE(h.contains("lidlToStdStringVector"));
}

TEST(LidlGenProvider, HeaderHasImplMember)
{
    auto m = makeTestModule();
    QString h = lidlMakeProviderHeader(m, "TestModuleImpl", "test_module_impl.h");
    EXPECT_TRUE(h.contains("TestModuleImpl m_impl"));
}

TEST(LidlGenProvider, HeaderHasCreateProviderObject)
{
    auto m = makeTestModule();
    QString h = lidlMakeProviderHeader(m, "TestModuleImpl", "test_module_impl.h");
    EXPECT_TRUE(h.contains("createProviderObject()"));
    EXPECT_TRUE(h.contains("new TestModuleProviderObject()"));
}

TEST(LidlGenProvider, HeaderPluginReturnsCorrectName)
{
    auto m = makeTestModule();
    QString h = lidlMakeProviderHeader(m, "TestModuleImpl", "test_module_impl.h");
    EXPECT_TRUE(h.contains("return QStringLiteral(\"test_module\")"));
    EXPECT_TRUE(h.contains("return QStringLiteral(\"1.0.0\")"));
}

// ---------------------------------------------------------------------------
// No string vec helpers when not needed
// ---------------------------------------------------------------------------

TEST(LidlGenProvider, NoStringVecHelpersWhenNotNeeded)
{
    ModuleDecl m;
    m.name = "simple";
    m.version = "1.0.0";
    MethodDecl md;
    md.name = "getFlag";
    md.returnType = { TypeExpr::Primitive, "bool", {} };
    m.methods.append(md);

    QString h = lidlMakeProviderHeader(m, "SimpleImpl", "simple_impl.h");
    EXPECT_FALSE(h.contains("lidlToQStringList"));
}

// ---------------------------------------------------------------------------
// Dispatch generation
// ---------------------------------------------------------------------------

TEST(LidlGenProvider, DispatchContainsCallMethod)
{
    auto m = makeTestModule();
    QString d = lidlMakeProviderDispatch(m);
    EXPECT_TRUE(d.contains("::callMethod(const QString& methodName, const QVariantList& args)"));
}

TEST(LidlGenProvider, DispatchHasMethodChecks)
{
    auto m = makeTestModule();
    QString d = lidlMakeProviderDispatch(m);
    EXPECT_TRUE(d.contains("if (methodName == \"greet\")"));
    EXPECT_TRUE(d.contains("if (methodName == \"isValid\")"));
    EXPECT_TRUE(d.contains("if (methodName == \"getCount\")"));
    EXPECT_TRUE(d.contains("if (methodName == \"getNames\")"));
    EXPECT_TRUE(d.contains("if (methodName == \"doNothing\")"));
}

TEST(LidlGenProvider, DispatchVoidReturnsTrueVariant)
{
    auto m = makeTestModule();
    QString d = lidlMakeProviderDispatch(m);
    // doNothing is void, dispatch should return QVariant(true)
    EXPECT_TRUE(d.contains("return QVariant(true)"));
}

TEST(LidlGenProvider, DispatchHasUnknownMethodWarning)
{
    auto m = makeTestModule();
    QString d = lidlMakeProviderDispatch(m);
    EXPECT_TRUE(d.contains("unknown method"));
}

TEST(LidlGenProvider, DispatchContainsGetMethods)
{
    auto m = makeTestModule();
    QString d = lidlMakeProviderDispatch(m);
    EXPECT_TRUE(d.contains("::getMethods()"));
    EXPECT_TRUE(d.contains("QJsonArray methods"));
}

TEST(LidlGenProvider, DispatchGetMethodsHasAllMethods)
{
    auto m = makeTestModule();
    QString d = lidlMakeProviderDispatch(m);
    EXPECT_TRUE(d.contains("\"greet\""));
    EXPECT_TRUE(d.contains("\"isValid\""));
    EXPECT_TRUE(d.contains("\"getCount\""));
    EXPECT_TRUE(d.contains("\"getNames\""));
    EXPECT_TRUE(d.contains("\"doNothing\""));
}

TEST(LidlGenProvider, DispatchGetMethodsHasSignature)
{
    auto m = makeTestModule();
    QString d = lidlMakeProviderDispatch(m);
    EXPECT_TRUE(d.contains("\"signature\""));
    EXPECT_TRUE(d.contains("greet(QString)"));
}

TEST(LidlGenProvider, DispatchGetMethodsHasReturnType)
{
    auto m = makeTestModule();
    QString d = lidlMakeProviderDispatch(m);
    EXPECT_TRUE(d.contains("\"returnType\""));
}

TEST(LidlGenProvider, DispatchGetMethodsHasParameters)
{
    auto m = makeTestModule();
    QString d = lidlMakeProviderDispatch(m);
    EXPECT_TRUE(d.contains("\"parameters\""));
}

TEST(LidlGenProvider, DispatchIncludesGlueHeader)
{
    auto m = makeTestModule();
    QString d = lidlMakeProviderDispatch(m);
    EXPECT_TRUE(d.contains("#include \"test_module_qt_glue.h\""));
}

// ---------------------------------------------------------------------------
// Events in provider header
// ---------------------------------------------------------------------------

TEST(LidlGenProvider, EventEmitters)
{
    ModuleDecl m;
    m.name = "evented";
    m.version = "1.0.0";
    EventDecl ed;
    ed.name = "onUpdate";
    ParamDecl p; p.name = "data"; p.type = { TypeExpr::Primitive, "tstr", {} };
    ed.params.append(p);
    m.events.append(ed);

    QString h = lidlMakeProviderHeader(m, "EventedImpl", "evented_impl.h");
    // The provider class still gains a protected `emit<EventName>(...)`
    // helper per declared event (callable from custom provider code,
    // unused by the standard impl flow which goes via the typed
    // `logos_events:` path).
    EXPECT_TRUE(h.contains("emitOnupdate"));
    EXPECT_TRUE(h.contains("emitEvent(\"onUpdate\""));
    // Events declared in ModuleDecl.events flow through the
    // `_logos_codegen_::maybeSetEmitEvent` SFINAE helper invoked from
    // the generated `onInit` — NOT the legacy `m_impl.emitEvent = …`
    // constructor wiring (that's gated on `hasEmitEvent` for back-compat
    // with un-migrated modules — see ConstructorWiresEmitEventWhenHasEmitEventOnly).
    EXPECT_TRUE(h.contains("_logos_codegen_::maybeSetEmitEvent(m_impl"));
    EXPECT_TRUE(h.contains("emitEvent(QString::fromStdString(name)"));
    EXPECT_FALSE(h.contains("m_impl.emitEvent ="));
}

TEST(LidlGenProvider, ConstructorWiresEmitEventWhenHasEmitEventOnly)
{
    ModuleDecl m;
    m.name = "emitonly";
    m.version = "1.0.0";
    m.hasEmitEvent = true;

    QString h = lidlMakeProviderHeader(m, "EmitOnlyImpl", "emitonly_impl.h");
    EXPECT_TRUE(h.contains("m_impl.emitEvent ="));
    EXPECT_TRUE(h.contains("emitEvent(QString::fromStdString(name)"));
}

TEST(LidlGenProvider, HeaderIncludesNlohmannConversionForJsonMapReturn)
{
    ModuleDecl m;
    m.name = "jm";
    m.version = "1.0.0";
    MethodDecl md;
    md.name = "getPayload";
    md.returnType = { TypeExpr::Map, "", { { TypeExpr::Primitive, "tstr", {} },
        { TypeExpr::Primitive, "any", {} } } };
    md.jsonReturn = true;
    m.methods.append(md);

    QString h = lidlMakeProviderHeader(m, "JmImpl", "jm_impl.h");
    EXPECT_TRUE(h.contains("#include <nlohmann/json.hpp>"));
    EXPECT_TRUE(h.contains("nlohmannToQVariant"));
    EXPECT_TRUE(h.contains("auto _result = m_impl.getPayload("));
    EXPECT_TRUE(h.contains("return nlohmannToQVariant(_result).toMap()"));
}

TEST(LidlGenProvider, HeaderUsesToListForJsonListReturn)
{
    ModuleDecl m;
    m.name = "jl";
    m.version = "1.0.0";
    MethodDecl md;
    md.name = "getItems";
    md.returnType = { TypeExpr::Array, "", { { TypeExpr::Primitive, "any", {} } } };
    md.jsonReturn = true;
    m.methods.append(md);

    QString h = lidlMakeProviderHeader(m, "JlImpl", "jl_impl.h");
    EXPECT_TRUE(h.contains("return nlohmannToQVariant(_result).toList()"));
}

TEST(LidlGenProvider, NoNlohmannBlockWithoutJsonReturnMethods)
{
    ModuleDecl m = makeTestModule();
    QString h = lidlMakeProviderHeader(m, "TestModuleImpl", "test_module_impl.h");
    EXPECT_FALSE(h.contains("nlohmann/json.hpp"));
    EXPECT_FALSE(h.contains("nlohmannToQVariant"));
}

TEST(LidlGenProvider, HeaderIncludesNlohmannAndStdResultForResultReturn)
{
    ModuleDecl m;
    m.name = "resmod";
    m.version = "1.0.0";
    MethodDecl md;
    md.name = "getResult";
    md.returnType = { TypeExpr::Primitive, "result", {} };
    md.resultReturn = true;
    m.methods.append(md);

    QString h = lidlMakeProviderHeader(m, "ResModImpl", "resmod_impl.h");
    EXPECT_TRUE(h.contains("#include <nlohmann/json.hpp>"));
    EXPECT_TRUE(h.contains("nlohmannToQVariant"));
    EXPECT_TRUE(h.contains("#include \"logos_result.h\""));
    EXPECT_TRUE(h.contains("stdResultToQt"));
    EXPECT_TRUE(h.contains("auto _result = m_impl.getResult("));
    EXPECT_TRUE(h.contains("return stdResultToQt(_result)"));
}

TEST(LidlGenProvider, StdResultMethodHasLogosResultReturnType)
{
    ModuleDecl m;
    m.name = "resmod2";
    m.version = "1.0.0";
    MethodDecl md;
    md.name = "doWork";
    md.returnType = { TypeExpr::Primitive, "result", {} };
    md.resultReturn = true;
    m.methods.append(md);

    QString h = lidlMakeProviderHeader(m, "ResModImpl2", "resmod2_impl.h");
    EXPECT_TRUE(h.contains("LogosResult doWork("));
}

// ---------------------------------------------------------------------------
// Empty module
// ---------------------------------------------------------------------------

TEST(LidlGenProvider, EmptyModuleGeneratesValidCode)
{
    ModuleDecl m;
    m.name = "empty";
    m.version = "0.0.1";

    QString h = lidlMakeProviderHeader(m, "EmptyImpl", "empty_impl.h");
    EXPECT_TRUE(h.contains("EmptyProviderObject"));
    EXPECT_TRUE(h.contains("EmptyPlugin"));

    QString d = lidlMakeProviderDispatch(m);
    EXPECT_TRUE(d.contains("::callMethod("));
    EXPECT_TRUE(d.contains("::getMethods()"));
}
