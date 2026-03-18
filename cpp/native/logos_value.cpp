#include "logos_value.h"

#include <sstream>
#include <stdexcept>
#include <cctype>

LogosValue::LogosValue() : m_data(std::monostate{}) {}

LogosValue::LogosValue(bool v) : m_data(v) {}

LogosValue::LogosValue(int v) : m_data(static_cast<int64_t>(v)) {}

LogosValue::LogosValue(int64_t v) : m_data(v) {}

LogosValue::LogosValue(double v) : m_data(v) {}

LogosValue::LogosValue(const std::string& v) : m_data(v) {}

LogosValue::LogosValue(const char* v) : m_data(std::string(v ? v : "")) {}

LogosValue::LogosValue(const List& v) : m_data(v) {}

LogosValue::LogosValue(const Map& v) : m_data(v) {}

LogosValue::LogosValue(const std::vector<std::string>& v)
{
    List list;
    list.reserve(v.size());
    for (const auto& s : v) {
        list.emplace_back(s);
    }
    m_data = std::move(list);
}

bool LogosValue::isNull() const { return std::holds_alternative<std::monostate>(m_data); }
bool LogosValue::isBool() const { return std::holds_alternative<bool>(m_data); }
bool LogosValue::isInt() const { return std::holds_alternative<int64_t>(m_data); }
bool LogosValue::isDouble() const { return std::holds_alternative<double>(m_data); }
bool LogosValue::isString() const { return std::holds_alternative<std::string>(m_data); }
bool LogosValue::isList() const { return std::holds_alternative<List>(m_data); }
bool LogosValue::isMap() const { return std::holds_alternative<Map>(m_data); }

bool LogosValue::toBool(bool def) const
{
    if (auto* v = std::get_if<bool>(&m_data)) return *v;
    if (auto* v = std::get_if<int64_t>(&m_data)) return *v != 0;
    if (auto* v = std::get_if<double>(&m_data)) return *v != 0.0;
    if (auto* v = std::get_if<std::string>(&m_data)) {
        if (*v == "true" || *v == "1") return true;
        if (*v == "false" || *v == "0") return false;
    }
    return def;
}

int64_t LogosValue::toInt(int64_t def) const
{
    if (auto* v = std::get_if<int64_t>(&m_data)) return *v;
    if (auto* v = std::get_if<bool>(&m_data)) return *v ? 1 : 0;
    if (auto* v = std::get_if<double>(&m_data)) return static_cast<int64_t>(*v);
    if (auto* v = std::get_if<std::string>(&m_data)) {
        try { return std::stoll(*v); } catch (...) {}
    }
    return def;
}

double LogosValue::toDouble(double def) const
{
    if (auto* v = std::get_if<double>(&m_data)) return *v;
    if (auto* v = std::get_if<int64_t>(&m_data)) return static_cast<double>(*v);
    if (auto* v = std::get_if<bool>(&m_data)) return *v ? 1.0 : 0.0;
    if (auto* v = std::get_if<std::string>(&m_data)) {
        try { return std::stod(*v); } catch (...) {}
    }
    return def;
}

std::string LogosValue::toString(const std::string& def) const
{
    if (auto* v = std::get_if<std::string>(&m_data)) return *v;
    if (auto* v = std::get_if<bool>(&m_data)) return *v ? "true" : "false";
    if (auto* v = std::get_if<int64_t>(&m_data)) return std::to_string(*v);
    if (auto* v = std::get_if<double>(&m_data)) return std::to_string(*v);
    if (isNull()) return def;
    return toJson();
}

LogosValue::List LogosValue::toList() const
{
    if (auto* v = std::get_if<List>(&m_data)) return *v;
    return {};
}

LogosValue::Map LogosValue::toMap() const
{
    if (auto* v = std::get_if<Map>(&m_data)) return *v;
    return {};
}

std::vector<std::string> LogosValue::toStringList() const
{
    std::vector<std::string> result;
    if (auto* v = std::get_if<List>(&m_data)) {
        result.reserve(v->size());
        for (const auto& item : *v) {
            result.push_back(item.toString());
        }
    }
    return result;
}

LogosValue::operator bool() const { return !isNull(); }

// --- JSON serialization ---

static void escapeJsonString(std::ostringstream& out, const std::string& s)
{
    out << '"';
    for (char c : s) {
        switch (c) {
        case '"':  out << "\\\""; break;
        case '\\': out << "\\\\"; break;
        case '\b': out << "\\b"; break;
        case '\f': out << "\\f"; break;
        case '\n': out << "\\n"; break;
        case '\r': out << "\\r"; break;
        case '\t': out << "\\t"; break;
        default:
            if (static_cast<unsigned char>(c) < 0x20) {
                char buf[8];
                snprintf(buf, sizeof(buf), "\\u%04x", static_cast<unsigned int>(c));
                out << buf;
            } else {
                out << c;
            }
        }
    }
    out << '"';
}

static void valueToJson(std::ostringstream& out, const LogosValue& val)
{
    if (val.isNull()) {
        out << "null";
    } else if (val.isBool()) {
        out << (val.toBool() ? "true" : "false");
    } else if (val.isInt()) {
        out << val.toInt();
    } else if (val.isDouble()) {
        out << val.toDouble();
    } else if (val.isString()) {
        escapeJsonString(out, val.toString());
    } else if (val.isList()) {
        out << '[';
        const auto& list = val.toList();
        for (size_t i = 0; i < list.size(); ++i) {
            if (i > 0) out << ',';
            valueToJson(out, list[i]);
        }
        out << ']';
    } else if (val.isMap()) {
        out << '{';
        const auto& map = val.toMap();
        bool first = true;
        for (const auto& [k, v] : map) {
            if (!first) out << ',';
            first = false;
            escapeJsonString(out, k);
            out << ':';
            valueToJson(out, v);
        }
        out << '}';
    }
}

std::string LogosValue::toJson() const
{
    std::ostringstream out;
    valueToJson(out, *this);
    return out.str();
}

// --- JSON parsing ---

namespace {

struct JsonParser {
    const std::string& src;
    size_t pos = 0;

    char peek() const { return pos < src.size() ? src[pos] : '\0'; }
    char advance() { return pos < src.size() ? src[pos++] : '\0'; }

    void skipWhitespace()
    {
        while (pos < src.size() && std::isspace(static_cast<unsigned char>(src[pos])))
            ++pos;
    }

    LogosValue parseValue()
    {
        skipWhitespace();
        char c = peek();
        if (c == '"') return parseString();
        if (c == '{') return parseObject();
        if (c == '[') return parseArray();
        if (c == 't' || c == 'f') return parseBool();
        if (c == 'n') return parseNull();
        return parseNumber();
    }

    LogosValue parseNull()
    {
        if (src.compare(pos, 4, "null") == 0) {
            pos += 4;
            return LogosValue();
        }
        throw std::runtime_error("Invalid JSON: expected null");
    }

    LogosValue parseBool()
    {
        if (src.compare(pos, 4, "true") == 0) {
            pos += 4;
            return LogosValue(true);
        }
        if (src.compare(pos, 5, "false") == 0) {
            pos += 5;
            return LogosValue(false);
        }
        throw std::runtime_error("Invalid JSON: expected true/false");
    }

    LogosValue parseNumber()
    {
        size_t start = pos;
        bool isFloat = false;
        if (peek() == '-') advance();
        while (pos < src.size() && std::isdigit(static_cast<unsigned char>(src[pos])))
            advance();
        if (peek() == '.') {
            isFloat = true;
            advance();
            while (pos < src.size() && std::isdigit(static_cast<unsigned char>(src[pos])))
                advance();
        }
        if (peek() == 'e' || peek() == 'E') {
            isFloat = true;
            advance();
            if (peek() == '+' || peek() == '-') advance();
            while (pos < src.size() && std::isdigit(static_cast<unsigned char>(src[pos])))
                advance();
        }
        std::string numStr = src.substr(start, pos - start);
        if (isFloat)
            return LogosValue(std::stod(numStr));
        return LogosValue(static_cast<int64_t>(std::stoll(numStr)));
    }

    std::string parseRawString()
    {
        advance(); // opening '"'
        std::string result;
        while (pos < src.size()) {
            char c = advance();
            if (c == '"') return result;
            if (c == '\\') {
                char esc = advance();
                switch (esc) {
                case '"': result += '"'; break;
                case '\\': result += '\\'; break;
                case '/': result += '/'; break;
                case 'b': result += '\b'; break;
                case 'f': result += '\f'; break;
                case 'n': result += '\n'; break;
                case 'r': result += '\r'; break;
                case 't': result += '\t'; break;
                case 'u': {
                    if (pos + 4 > src.size())
                        throw std::runtime_error("Invalid JSON: incomplete \\u escape");
                    std::string hex = src.substr(pos, 4);
                    pos += 4;
                    unsigned int cp = static_cast<unsigned int>(std::stoul(hex, nullptr, 16));
                    if (cp < 0x80) {
                        result += static_cast<char>(cp);
                    } else if (cp < 0x800) {
                        result += static_cast<char>(0xC0 | (cp >> 6));
                        result += static_cast<char>(0x80 | (cp & 0x3F));
                    } else {
                        result += static_cast<char>(0xE0 | (cp >> 12));
                        result += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
                        result += static_cast<char>(0x80 | (cp & 0x3F));
                    }
                    break;
                }
                default: result += esc;
                }
            } else {
                result += c;
            }
        }
        throw std::runtime_error("Invalid JSON: unterminated string");
    }

    LogosValue parseString()
    {
        return LogosValue(parseRawString());
    }

    LogosValue parseArray()
    {
        advance(); // '['
        LogosValue::List list;
        skipWhitespace();
        if (peek() == ']') { advance(); return LogosValue(list); }
        while (true) {
            list.push_back(parseValue());
            skipWhitespace();
            if (peek() == ']') { advance(); return LogosValue(list); }
            if (peek() != ',')
                throw std::runtime_error("Invalid JSON: expected ',' or ']' in array");
            advance();
        }
    }

    LogosValue parseObject()
    {
        advance(); // '{'
        LogosValue::Map map;
        skipWhitespace();
        if (peek() == '}') { advance(); return LogosValue(map); }
        while (true) {
            skipWhitespace();
            if (peek() != '"')
                throw std::runtime_error("Invalid JSON: expected string key in object");
            std::string key = parseRawString();
            skipWhitespace();
            if (peek() != ':')
                throw std::runtime_error("Invalid JSON: expected ':' after key");
            advance();
            map[key] = parseValue();
            skipWhitespace();
            if (peek() == '}') { advance(); return LogosValue(map); }
            if (peek() != ',')
                throw std::runtime_error("Invalid JSON: expected ',' or '}' in object");
            advance();
        }
    }
};

} // anonymous namespace

LogosValue LogosValue::fromJson(const std::string& json)
{
    JsonParser parser{json, 0};
    LogosValue val = parser.parseValue();
    return val;
}
