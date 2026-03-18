#ifndef LOGOS_VALUE_H
#define LOGOS_VALUE_H

#include <cstdint>
#include <map>
#include <string>
#include <variant>
#include <vector>

class LogosValue {
public:
    using List = std::vector<LogosValue>;
    using Map = std::map<std::string, LogosValue>;

private:
    std::variant<std::monostate, bool, int64_t, double, std::string, List, Map> m_data;

public:
    LogosValue();
    LogosValue(bool v);
    LogosValue(int v);
    LogosValue(int64_t v);
    LogosValue(double v);
    LogosValue(const std::string& v);
    LogosValue(const char* v);
    LogosValue(const List& v);
    LogosValue(const Map& v);
    LogosValue(const std::vector<std::string>& v);

    bool isNull() const;
    bool isBool() const;
    bool isInt() const;
    bool isDouble() const;
    bool isString() const;
    bool isList() const;
    bool isMap() const;

    bool toBool(bool def = false) const;
    int64_t toInt(int64_t def = 0) const;
    double toDouble(double def = 0.0) const;
    std::string toString(const std::string& def = "") const;
    List toList() const;
    Map toMap() const;
    std::vector<std::string> toStringList() const;

    std::string toJson() const;
    static LogosValue fromJson(const std::string& json);

    explicit operator bool() const;
};

#endif // LOGOS_VALUE_H
