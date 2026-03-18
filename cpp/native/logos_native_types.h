#ifndef LOGOS_NATIVE_TYPES_H
#define LOGOS_NATIVE_TYPES_H

#include "logos_value.h"

#include <stdexcept>
#include <string>

class NativeLogosResultException : public std::runtime_error
{
public:
    using std::runtime_error::runtime_error;
};

struct NativeLogosResult
{
    bool success = false;
    LogosValue value;
    std::string error;

    std::string getString() const
    {
        if (!success)
            throw NativeLogosResultException("Attempted to get value from a failed NativeLogosResult: " + error);
        return value.toString();
    }

    bool getBool() const
    {
        if (!success)
            throw NativeLogosResultException("Attempted to get value from a failed NativeLogosResult: " + error);
        return value.toBool();
    }

    int getInt() const
    {
        if (!success)
            throw NativeLogosResultException("Attempted to get value from a failed NativeLogosResult: " + error);
        return static_cast<int>(value.toInt());
    }

    LogosValue::List getList() const
    {
        if (!success)
            throw NativeLogosResultException("Attempted to get value from a failed NativeLogosResult: " + error);
        return value.toList();
    }

    LogosValue::Map getMap() const
    {
        if (!success)
            throw NativeLogosResultException("Attempted to get value from a failed NativeLogosResult: " + error);
        return value.toMap();
    }

    std::string getString(const std::string& key, const std::string& def = "") const
    {
        if (!success)
            throw NativeLogosResultException("Attempted to get value from a failed NativeLogosResult: " + error);
        auto map = value.toMap();
        auto it = map.find(key);
        if (it == map.end()) return def;
        return it->second.toString(def);
    }

    bool getBool(const std::string& key, bool def = false) const
    {
        if (!success)
            throw NativeLogosResultException("Attempted to get value from a failed NativeLogosResult: " + error);
        auto map = value.toMap();
        auto it = map.find(key);
        if (it == map.end()) return def;
        return it->second.toBool(def);
    }

    int getInt(const std::string& key, int def = 0) const
    {
        if (!success)
            throw NativeLogosResultException("Attempted to get value from a failed NativeLogosResult: " + error);
        auto map = value.toMap();
        auto it = map.find(key);
        if (it == map.end()) return def;
        return static_cast<int>(it->second.toInt(def));
    }

    std::string getError() const
    {
        if (success)
            throw NativeLogosResultException("Attempted to get error from a successful NativeLogosResult");
        return error;
    }
};

#endif // LOGOS_NATIVE_TYPES_H
