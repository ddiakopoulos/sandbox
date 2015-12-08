// See COPYING file for attribution information

#ifndef json_h
#define json_h

#include <cstdint>
#include <cassert>
#include <sstream>
#include <vector>

namespace util
{
    class JsonValue;
    typedef std::vector<JsonValue> JsonArray;
    typedef std::vector<std::pair<std::string, JsonValue>> JsonObject;
    struct JsonParseError : std::runtime_error { JsonParseError(const std::string & what) : runtime_error("json parse error - " + what) {} };

    JsonValue jsonFrom(const std::string & text); // throws JsonParseError
    bool isJsonNumber(const std::string & num);

    class JsonValue
    {
        template<class T> static std::string to_str(const T & val) { std::ostringstream ss; ss << val; return ss.str(); }

        enum                Kind { Null, False, True, String, Number, Array, Object };
        Kind                kind; // What kind of value is this?
        std::string         str;  // Contents of String or Number value
        JsonObject          obj;  // Fields of Object value
        JsonArray           arr;  // Elements of Array value

                            JsonValue(Kind kind, std::string str)       : kind(kind), str(move(str)) {}
    public:
                            JsonValue()                                 : kind(Null) {}                 // Default construct null
                            JsonValue(std::nullptr_t)                   : kind(Null) {}                 // Construct null from nullptr
                            JsonValue(bool b)                           : kind(b ? True : False) {}     // Construct true or false from boolean
                            JsonValue(const char * s)                   : JsonValue(String, s) {}           // Construct String from C-string
                            JsonValue(std::string s)                    : JsonValue(String, move(s)) {}     // Construct String from std::string
                            JsonValue(int32_t n)                        : JsonValue(Number, to_str(n)) {}   // Construct Number from integer
                            JsonValue(uint32_t n)                       : JsonValue(Number, to_str(n)) {}   // Construct Number from integer
                            JsonValue(int64_t n)                        : JsonValue(Number, to_str(n)) {}   // Construct Number from integer
                            JsonValue(uint64_t n)                       : JsonValue(Number, to_str(n)) {}   // Construct Number from integer
                            JsonValue(float n)                          : JsonValue(Number, to_str(n)) {}   // Construct Number from float
                            JsonValue(double n)                         : JsonValue(Number, to_str(n)) {}   // Construct Number from double
                            JsonValue(JsonObject o)                     : kind(Object), obj(move(o)) {} // Construct Object from vector<pair<string,JsonValue>> (TODO: Assert no duplicate keys)
                            JsonValue(JsonArray a)                      : kind(Array), arr(move(a)) {}  // Construct Array from vector<JsonValue>

        bool                operator == (const JsonValue & r) const     { return kind == r.kind && str == r.str && obj == r.obj && arr == r.arr; }
        bool                operator != (const JsonValue & r) const     { return !(*this == r); }

        const JsonValue &   operator[](size_t index) const              { const static JsonValue null; return index < arr.size() ? arr[index] : null; }
        const JsonValue &   operator[](int index) const                 { const static JsonValue null; return index < 0 ? null : (*this)[static_cast<size_t>(index)]; }
        const JsonValue &   operator[](const char * key) const          { for (auto & kvp : obj) if (kvp.first == key) return kvp.second; const static JsonValue null; return null; }
        const JsonValue &   operator[](const std::string & key) const   { return (*this)[key.c_str()]; }

        bool                isString() const                            { return kind == String; }
        bool                isNumber() const                            { return kind == Number; }
        bool                isObject() const                            { return kind == Object; }
        bool                isArray() const                             { return kind == Array; }
        bool                isTrue() const                              { return kind == True; }
        bool                isFalse() const                             { return kind == False; }
        bool                isNull() const                              { return kind == Null; }

        bool                boolOrDefault(bool def) const               { return isTrue() ? true : isFalse() ? false : def; }
        std::string         stringOrDefault(const char * def) const     { return kind == String ? str : def; }
        template<class T> T numberOrDefault(T def) const                { if (!isNumber()) return def; T val = def; std::istringstream(str) >> val; return val; }

        std::string         string() const                              { return stringOrDefault(""); } // Value, if a String, empty otherwise
        template<class T> T number() const                              { return numberOrDefault(T()); } // Value, if a Number, empty otherwise
        const JsonObject &  object() const                              { return obj; }    // Name/value pairs, if an Object, empty otherwise
        const JsonArray &   array() const                               { return arr; }    // Values, if an Array, empty otherwise

        const std::string & contents() const                            { return str; }    // Contents, if a String, JSON format number, if a Number, empty otherwise

        static JsonValue    fromNumber(std::string num)                 { assert(util::isJsonNumber(num)); return JsonValue(Number, move(num)); }
    };

    std::ostream & operator << (std::ostream & out, const JsonValue & val);
    std::ostream & operator << (std::ostream & out, const JsonArray & arr);
    std::ostream & operator << (std::ostream & out, const JsonObject & obj);

    template<class T> struct tabbed_ref { const T & value; int tabWidth, indent; };
    template<class T> tabbed_ref<T> tabbed(const T & value, int tabWidth, int indent = 0) { return{ value, tabWidth, indent }; }
    std::ostream & operator << (std::ostream & out, tabbed_ref<JsonValue> val);
    std::ostream & operator << (std::ostream & out, tabbed_ref<JsonArray> arr);
    std::ostream & operator << (std::ostream & out, tabbed_ref<JsonObject> obj);

} // end namespace util

#endif // end json_h