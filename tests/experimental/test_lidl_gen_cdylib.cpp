#include <gtest/gtest.h>

#include "lidl_gen_cdylib.h"

TEST(LidlGenCdylib, BinaryEventPayloadUsesCanonicalBytesEncoding)
{
    ModuleDecl module;
    module.name = "delivery_module";

    EventDecl event;
    event.name = "messageReceived";

    ParamDecl payload;
    payload.name = "payload";
    payload.type = {TypeExpr::Primitive, "bstr", {}};
    event.params.push_back(payload);
    module.events.push_back(event);

    const QString source = lidlMakeEventsSourceCdylib(
        module,
        "DeliveryModuleImpl",
        "delivery_module_plugin.h");

    EXPECT_TRUE(source.contains("args.push_back(lidlBytesToJson(payload));"));
    EXPECT_TRUE(source.contains("std::string lidlB64UrlEncode"));
    EXPECT_TRUE(source.contains("nlohmann::json lidlBytesToJson"));
    EXPECT_FALSE(source.contains("nlohmann::json{{\"_bytes\", \"\"}}"));
}
