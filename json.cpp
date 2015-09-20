#include "json.hpp"
#include <algorithm>
#include <regex>

namespace util
{

std::ostream & printEscaped(std::ostream & out, const std::string & str)
{
    // Escape sequences for ", \, and control characters, 0 indicates no escaping needed
    static const char * escapes[256] = {
        "\\u0000", "\\u0001", "\\u0002", "\\u0003", "\\u0004", "\\u0005", "\\u0006", "\\u0007",
        "\\b", "\\t", "\\n", "\\u000B", "\\f", "\\r", "\\u000E", "\\u000F",
        "\\u0010", "\\u0011", "\\u0012", "\\u0013", "\\u0014", "\\u0015", "\\u0016", "\\u0017",
        "\\u0018", "\\u0019", "\\u001A", "\\u001B", "\\u001C", "\\u001D", "\\u001E", "\\u001F",
        0, 0, "\\\"", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, "\\\\", 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, "\\u007F"
    };
    out << '"';
    for (uint8_t ch : str)
    {
        if (escapes[ch]) out << escapes[ch];
        else out << ch;
    }
    return out << '"';
}

std::ostream & operator << (std::ostream & out, const JsonArray & arr)
{
    int i = 0;
    out << '[';
    for (auto & val : arr) out << (i++ ? "," : "") << val;
    return out << ']';
}

std::ostream & operator << (std::ostream & out, const JsonObject & obj)
{
    int i = 0;
    out << '{';
    for (auto & kvp : obj)
    {
        printEscaped(out << (i++ ? "," : ""), kvp.first) << ':' << kvp.second;
    }
    return out << '}';
}

std::ostream & operator << (std::ostream & out, const JsonValue & val)
{
    if (val.isNull()) return out << "null";
    else if (val.isFalse()) return out << "false";
    else if (val.isTrue()) return out << "true";
    else if (val.isString()) return printEscaped(out, val.contents());
    else if (val.isNumber()) return out << val.contents();
    else if (val.isArray()) return out << val.array();
    else return out << val.object();
}

static std::ostream & indent(std::ostream & out, int space, int n = 0)
{
    if (n) out << ',';
    out << '\n';
    for (int i = 0; i < space; ++i) out << ' ';
    return out;
}

std::ostream & operator << (std::ostream & out, tabbed_ref<JsonArray> arr)
{
    if (std::none_of(begin(arr.value), end(arr.value), [](const JsonValue & val) { return val.isArray() || val.isObject(); })) return out << arr.value;
    else
    {
        int space = arr.indent + arr.tabWidth, i = 0;
        out << '[';
        for (auto & val : arr.value) indent(out, space, i++) << tabbed(val, arr.tabWidth, space);
        return indent(out, arr.indent) << ']';
    }
}

std::ostream & operator << (std::ostream & out, tabbed_ref<JsonObject> obj)
{
    if (obj.value.empty()) return out << "{}";
    else
    {
        int space = obj.indent + obj.tabWidth, i = 0;
        out << '{';
        for (auto & kvp : obj.value)
        {
            printEscaped(indent(out, space, i++), kvp.first) << ": " << tabbed(kvp.second, obj.tabWidth, space);
        }
        return indent(out, obj.indent) << '}';
    }
}

std::ostream & operator << (std::ostream & out, tabbed_ref<JsonValue> val)
{
    if (val.value.isArray()) return out << tabbed(val.value.array(), val.tabWidth, val.indent);
    else if (val.value.isObject()) return out << tabbed(val.value.object(), val.tabWidth, val.indent);
    else return out << val.value;
}

bool isJsonNumber(const std::string & num)
{
#if defined(_msc_ver)
    static const std::regex regex(R"(-?(0|([1-9][0-9]*))((\.[0-9]+)?)(((e|E)((\+|-)?)[0-9]+)?))");
    return std::regex_match(begin(num), end(num), regex);
#else
    return true; // regex_match does not work on GCC 4.8
#endif
}

static uint16_t decode_hex(char ch) {
    if (ch >= '0' && ch <= '9') return ch - '0';
    if (ch >= 'A' && ch <= 'F') return 10 + ch - 'A';
    if (ch >= 'a' && ch <= 'f') return 10 + ch - 'a';
    throw JsonParseError(std::string("invalid hex digit: ") + ch);
}

static std::string decode_string(std::string::const_iterator first, std::string::const_iterator last)
{
    if (std::any_of(first, last, iscntrl)) throw JsonParseError("control character found in string literal");
    if (std::find(first, last, '\\') == last) return std::string(first, last); // No escape characters, use the string directly
    std::string s; s.reserve(last - first); // Reserve enough memory to hold the entire string
    for (; first < last; ++first)
    {
        if (*first != '\\') s.push_back(*first);
        else switch (*(++first))
        {
        case '"': s.push_back('"'); break;
        case '\\': s.push_back('\\'); break;
        case '/': s.push_back('/'); break;
        case 'b': s.push_back('\b'); break;
        case 'f': s.push_back('\f'); break;
        case 'n': s.push_back('\n'); break;
        case 'r': s.push_back('\r'); break;
        case 't': s.push_back('\t'); break;
        case 'u':
            if (first + 5 > last) throw JsonParseError("incomplete escape sequence: " + std::string(first - 1, last));
            else
            {
                uint16_t val = (decode_hex(first[1]) << 12) | (decode_hex(first[2]) << 8) | (decode_hex(first[3]) << 4) | decode_hex(first[4]);
                if (val < 0x80) s.push_back(static_cast<char>(val)); // ASCII codepoint, no translation needed
                else if (val < 0x800) // 2-byte UTF-8 encoding
                {
                    s.push_back(0xC0 | ((val >> 6) & 0x1F)); // Leading byte: 5 content bits
                    s.push_back(0x80 | ((val >> 0) & 0x3F)); // Continuation byte: 6 content bits
                }
                else // 3-byte UTF-8 encoding (16 content bits, sufficient to store all \uXXXX patterns)
                {
                    s.push_back(0xE0 | ((val >> 12) & 0x0F)); // Leading byte: 4 content bits
                    s.push_back(0x80 | ((val >> 6) & 0x3F)); // Continuation byte: 6 content bits
                    s.push_back(0x80 | ((val >> 0) & 0x3F)); // Continuation byte: 6 content bits
                }
                first += 4;
            }
            break;
        default: throw JsonParseError("invalid escape sequence");
        }
    }
    return s;
}

struct JsonToken { char type; std::string value; JsonToken(char type, std::string value = std::string()) : type(type), value(move(value)) {} };

struct JsonParseState
{
    std::vector<JsonToken>::iterator it, last;

    bool matchAndDiscard(char type) { if (it->type != type) return false; ++it; return true; }
    void discardExpected(char type, const char * what) { if (!matchAndDiscard(type)) throw JsonParseError(std::string("Syntax error: Expected ") + what); }

    JsonValue parseValue()
    {
        auto token = it++;
        switch (token->type)
        {
        case 'n': return nullptr;
        case 'f': return false;
        case 't': return true;
        case '"': return token->value;
        case '#': return JsonValue::fromNumber(token->value);
        case '[':
            if (matchAndDiscard(']')) return JsonArray{};
            else
            {
                JsonArray arr;
                while (true)
                {
                    arr.push_back(parseValue());
                    if (matchAndDiscard(']')) return arr;
                    discardExpected(',', ", or ]");
                }
            }
        case '{':
            if (matchAndDiscard('}')) return JsonObject{};
            else
            {
                JsonObject obj;
                while (true)
                {
                    auto name = move(it->value);
                    discardExpected('"', "string");
                    discardExpected(':', ":");
                    obj.emplace_back(move(name), parseValue());
                    if (matchAndDiscard('}')) { return obj; }
                    discardExpected(',', ", or }");
                }
            }
        default: throw JsonParseError("Expected value");
        }
    }
};

std::vector<JsonToken> jsonTokensFrom(const std::string & text)
{
    std::vector<JsonToken> tokens;
    auto it = begin(text);
    while (true)
    {
        it = std::find_if_not(it, end(text), isspace); // Skip whitespace
        if (it == end(text))
        {
            tokens.emplace_back('$');
            return tokens;
        }
        switch (*it)
        {
        case '[': case ']': case ',':
        case '{': case '}': case ':':
            tokens.push_back({ *it++ });
            break;
        case '"':
            {
                auto it2 = ++it;
                for (; it2 < end(text); ++it2)
                {
                    if (*it2 == '"') break;
                    if (*it2 == '\\') ++it2;
                }
                if (it2 < end(text))
                {
                    tokens.emplace_back('"', decode_string(it, it2));
                    it = it2 + 1;
                }
                else throw JsonParseError("String missing closing quote");
            }
            break;
        case '-': case '0': case '1': case '2':
        case '3': case '4': case '5': case '6':
        case '7': case '8': case '9':
            {
                auto it2 = std::find_if_not(it, end(text), [](char ch) { return isalnum(ch) || ch == '+' || ch == '-' || ch == '.'; });
                auto num = std::string(it, it2);
                if (!isJsonNumber(num)) throw JsonParseError("Invalid number: " + num);
                tokens.emplace_back('#', move(num));
                it = it2;
            }
            break;
        default:
            if (isalpha(*it))
            {
                auto it2 = std::find_if_not(it, end(text), isalpha);
                if (std::equal(it, it2, "true")) tokens.emplace_back('t');
                else if (std::equal(it, it2, "false")) tokens.emplace_back('f');
                else if (std::equal(it, it2, "null")) tokens.emplace_back('n');
                else throw JsonParseError("Invalid token: " + std::string(it, it2));
                it = it2;
            }
            else throw JsonParseError("Invalid character: \'" + std::string(1, *it) + '"');
        }
    }
}

JsonValue jsonFrom(const std::string & text)
{
    auto tokens = jsonTokensFrom(text);
    JsonParseState p = { begin(tokens), end(tokens) };
    auto val = p.parseValue();
    p.discardExpected('$', "end-of-stream");
    return val;
}

}