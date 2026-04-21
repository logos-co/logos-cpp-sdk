#pragma once
#include <string>
#include <nlohmann/json.hpp>

// Pure C++ result type for use in universal module implementations.
// No Qt dependency. The code generator recognizes "StdLogosResult" and emits
// a StdLogosResult -> Qt LogosResult conversion in the glue layer, so callers
// continue to receive the Qt LogosResult they expect.
//
// Usage in impl:
//   StdLogosResult myMethod() {
//       if (error) return {false, {}, "something went wrong"};
//       return {true, "some string value"};   // json accepts string, number, object, array
//   }
struct StdLogosResult {
    bool success = false;
    nlohmann::json value;   // any JSON value: string, number, bool, object, array, null
    std::string error;
};
