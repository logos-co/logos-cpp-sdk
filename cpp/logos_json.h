#pragma once
#include <nlohmann/json.hpp>

// Semantic aliases for nlohmann::json used in universal module impl classes.
// The code generator recognizes these names and emits QVariantMap / QVariantList
// conversions in the Qt glue layer, so impl classes remain Qt-free.
using LogosMap  = nlohmann::json;
using LogosList = nlohmann::json;
