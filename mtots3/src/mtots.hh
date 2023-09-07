#include <cstdint>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

namespace mtots {

using i8 = std::int8_t;
using i16 = std::int16_t;
using i32 = std::int32_t;
using i64 = std::int64_t;
using u8 = std::uint8_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;

class Error final {
 public:
  std::string message;
  Error(const std::string &s) : message(s) {}
  Error(std::string &&s) : message(std::move(s)) {}
};

[[noreturn]] inline void panic(std::function<void(std::ostream &)> f) {
  std::stringstream ss;
  f(ss);
  throw Error(ss.str());
}

[[noreturn]] inline void panic(const std::string &message) {
  panic([&](auto &out) {
    out << message;
  });
}

class Symbol;

class SymbolData final {
  friend Symbol intern(const std::string &);
  SymbolData(const std::string &s) : string(s), hash(std::hash<std::string>{}(s)) {}

 public:
  const std::string string;
  const size_t hash;
};

class Symbol final {
  friend Symbol intern(const std::string &);
  Symbol(SymbolData *d) : data(d) {}

  SymbolData *data;

 public:
  bool operator==(Symbol rhs) const {
    return data == rhs.data;
  }

  bool operator<(Symbol rhs) const {
    return data->string < rhs.data->string;
  }

  size_t getHash() const {
    return data->hash;
  }

  const std::string &get() const {
    return data->string;
  }
};

inline std::ostream &repr(std::ostream &out, Symbol s) {
  return out << "<symbol " << s.get() << ">";
}

inline std::ostream &operator<<(std::ostream &out, Symbol s) {
  return out << s.get();
}

template <class T>
class Rc;

class String;
class List;
class Dict;

using Value = std::variant<
    std::monostate,
    bool,
    double,
    Symbol,
    Rc<String>,
    Rc<List>,
    Rc<Dict>>;
}  // namespace mtots

namespace std {
template <>
struct hash<::mtots::Value> {
  size_t operator()(const ::mtots::Value &x) const;
};
template <>
struct hash<::mtots::Symbol> {
  size_t operator()(const ::mtots::Symbol &x) const {
    return x.getHash();
  }
};
}  // namespace std

namespace mtots {

Symbol intern(const std::string &s) {
  static auto *map = new std::unordered_map<std::string, Symbol>();
  auto pair = map->find(s);
  if (pair == map->end()) {
    auto newSymbol = Symbol(new SymbolData(s));
    map->insert(std::make_pair(s, newSymbol));
    return newSymbol;
  } else {
    return pair->second;
  }
}

template <class T>
inline void hash_combine(std::size_t &seed, const T &v) {
  std::hash<T> hasher;
  seed ^= hasher(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
}

inline std::ostream &operator<<(std::ostream &out, const Value &value);

template <class T>
class Rc {
  friend class String;
  friend class List;
  friend class Dict;
  T *ptr;
  Rc(T *p) : ptr(p) {}

  void incr() noexcept {
    if (ptr) {
      ptr->refcnt++;
    }
  }

  void decr() {
    if (ptr) {
      if (ptr->refcnt) {
        ptr->refcnt--;
      } else {
        delete ptr;
      }
    }
  }

 public:
  constexpr Rc() noexcept : ptr(nullptr) {}
  Rc(const Rc<T> &rc) noexcept : ptr(rc.ptr) { incr(); }
  Rc(Rc<T> &&rc) noexcept : ptr(rc.ptr) { rc.ptr = nullptr; }
  ~Rc() { decr(); }

  Rc<T> &operator=(Rc<T> &&rhs) {
    decr();
    ptr = rhs.ptr;
  }

  Rc<T> &operator=(const Rc<T> &rhs) {
    rhs.incr();
    decr();
    ptr = rhs.ptr;
  }

  T &operator*() const noexcept {
    return *ptr;
  }
  T *operator->() const noexcept {
    return ptr;
  }

  bool operator==(const Rc<T> &rhs) const {
    return *ptr == *rhs;
  }
};

inline std::ostream &repr(std::ostream &out, const Value &value);
inline std::ostream &repr(std::ostream &out, std::monostate x) {
  return out << "nil";
}

inline std::ostream &repr(std::ostream &out, bool x) {
  return out << (x ? "true" : "false");
}

inline std::ostream &repr(std::ostream &out, double x) {
  return out << x;
}

inline std::ostream &repr(std::ostream &out, const String &x);
inline std::ostream &repr(std::ostream &out, const List &x);
inline std::ostream &repr(std::ostream &out, const Dict &x);

template <class T>
inline std::ostream &repr(std::ostream &out, const Rc<T> &x) {
  return repr(out, *x);
}

template <class T>
std::ostream &operator<<(std::ostream &out, const Rc<T> &rc) {
  return out << *rc;
}

class Base {
  template <class T>
  friend class Rc;
  u32 refcnt = 0;
};

class String final : public Base {
  std::string value;
  mutable bool frozen = false;
  mutable size_t hash = 0;

  String() {}
  String(std::string &&str) : value(std::move(str)) {}

 public:
  static Rc<String> make(std::string &&str) {
    return Rc<String>(new String(std::move(str)));
  }
  const std::string &get() const {
    return value;
  }
  void freeze() const {
    if (frozen) {
      return;
    }
    frozen = true;
    hash = std::hash<std::string>{}(value);
  }
  size_t getHash() const {
    freeze();
    return hash;
  }
  bool operator==(const String &rhs) const {
    if (this == &rhs) {
      return true;
    }
    return value == rhs.value;
  }
};

inline std::ostream &repr(std::ostream &out, const String &x) {
  out << '"';
  for (auto ch : x.get()) {
    switch (ch) {
      case '\n':
        out << "\\n";
        break;
      case '\t':
        out << "\\t";
        break;
      case '\0':
        out << "\\0";
        break;
      case '"':
        out << '"';
        break;
      default:
        out << ch;
        break;
    }
  }
  return out << '"';
}

inline std::ostream &operator<<(std::ostream &out, const String &x) {
  return out << x.get();
}

class List final : public Base {
  std::vector<Value> value;
  mutable bool frozen = false;
  mutable size_t hash = 0;

  List() {}
  List(std::vector<Value> &&vec) : value(std::move(vec)) {}

 public:
  static Rc<List> make(std::vector<Value> &&vec) {
    return Rc<List>(new List(std::move(vec)));
  }

  Value &operator[](size_t pos) {
    return value[pos];
  }

  const Value &operator[](size_t pos) const {
    return value[pos];
  }

  const std::vector<Value> &get() const {
    return value;
  }

  void freeze() const {
    if (frozen) {
      return;
    }
    frozen = true;
    hash = value.size();
    for (const auto &x : value) {
      hash_combine(hash, x);
    }
  }

  size_t getHash() const {
    freeze();
    return hash;
  }

  bool operator==(const List &rhs) const {
    if (this == &rhs) {
      return true;
    }
    return value == rhs.value;
  }
};

inline std::ostream &repr(std::ostream &out, const List &x) {
  bool first = true;
  out << '[';
  for (const auto &item : x.get()) {
    if (!first) {
      out << ", ";
    }
    first = false;
    repr(out, item);
  }
  return out << ']';
}

inline std::ostream &operator<<(std::ostream &out, const List &x) {
  return repr(out, x);
}

class Dict final : public Base {
  std::unordered_map<Value, Value> value;
  mutable bool frozen = false;
  mutable size_t hash = 0;

  Dict() {}
  Dict(std::unordered_map<Value, Value> &&map) : value(std::move(map)) {}

 public:
  static Rc<Dict> make(std::unordered_map<Value, Value> &&map) {
    return Rc<Dict>(new Dict(std::move(map)));
  }

  void freeze() const {
    if (frozen) {
      return;
    }
    frozen = true;
    size_t fullHash = value.size();
    for (const auto &pair : value) {
      auto pairHash = std::hash<Value>{}(pair.first);
      hash_combine(pairHash, pair.second);
      fullHash ^= pairHash;
    }
    hash = fullHash;
  }

  size_t getHash() const {
    freeze();
    return hash;
  }

  bool operator==(const Dict &rhs) const {
    return value == rhs.value;
  }

  const std::unordered_map<Value, Value> &get() const {
    return value;
  }
};

inline std::ostream &repr(std::ostream &out, const Dict &x) {
  bool first = true;
  out << '{';
  for (const auto &pair : x.get()) {
    if (!first) {
      out << ", ";
    }
    first = false;
    repr(out, pair.first);
    out << ": ";
    repr(out, pair.second);
  }
  return out << '}';
}

inline std::ostream &operator<<(std::ostream &out, const Dict &x) {
  return repr(out, x);
}

inline std::ostream &repr(std::ostream &out, const Value &value) {
  return std::visit(
      [&](const auto &x) -> std::ostream & {
        return repr(out, x);
      },
      value);
}

inline std::ostream &operator<<(std::ostream &out, const Value &value) {
  return std::visit(
      [&](const auto &x) -> std::ostream & {
        return out << x;
      },
      value);
}

}  // namespace mtots

namespace std {

template <class T>
struct hash<::mtots::Rc<T>> {
  size_t operator()(const ::mtots::Rc<T> &x) const {
    return x->getHash();
  }
};

inline size_t hash<::mtots::Value>::operator()(const ::mtots::Value &value) const {
  return std::visit(
      [&](const auto &x) -> size_t {
        return std::hash<std::decay_t<decltype(x)>>{}(x);
      },
      value);
}

}  // namespace std

namespace mtots {

/**
 * Literal class type that wraps a constant expression string.
 *
 * Uses implicit conversion to allow templates to *seemingly* accept constant strings.
 *
 * Delimiters, Operators and Keywords
 */
template <size_t N>
struct Key final {
  constexpr Key(const char (&str)[N]) {
    std::copy_n(str, N, value);
  }

  char value[N];
};

// Explicit class template argument deduction guide
// to suppress "-Wctad-maybe-unsupported" warnings
template <size_t N>
Key(const char (&str)[N]) -> Key<N>;

class TokenBase {
 public:
  u32 line;
  TokenBase(u32 ln) : line(ln) {}
};

/// @brief Token with no value field
template <Key K>
class KeyToken final : public TokenBase {
 public:
  static const char *getTypeName() {
    return K.value;
  }
};

template <Key K, class Arg>
class ArgToken final : public TokenBase {
 public:
  Arg value;
  ArgToken(u32 ln, Arg &&arg) : TokenBase(ln), value(std::move(arg)) {}
};

using EOFToken = KeyToken<"EOF">;
using NewlineToken = KeyToken<"NEWLINE">;
using IndentToken = KeyToken<"INDENT">;
using DedentToken = KeyToken<"DEDENT">;

using IdentifierToken = ArgToken<"IDENTIFIER", std::string>;
using StringToken = ArgToken<"STRING", std::string>;
using NumberToken = ArgToken<"NUMBER", double>;

// List of tokens more or less as described in
// https://docs.python.org/3/reference/lexical_analysis.html
using Token = std::variant<
    EOFToken,
    NewlineToken,
    IndentToken,
    DedentToken,
    IdentifierToken,

    // Keywords
    KeyToken<"False">,
    KeyToken<"await">,
    KeyToken<"else">,
    KeyToken<"import">,
    KeyToken<"pass">,
    KeyToken<"None">,
    KeyToken<"break">,
    KeyToken<"except">,
    KeyToken<"in">,
    KeyToken<"raise">,
    KeyToken<"True">,
    KeyToken<"class">,
    KeyToken<"finally">,
    KeyToken<"is">,
    KeyToken<"return">,
    KeyToken<"and">,
    KeyToken<"continue">,
    KeyToken<"for">,
    KeyToken<"lambda">,
    KeyToken<"try">,
    KeyToken<"as">,
    KeyToken<"def">,
    KeyToken<"from">,
    KeyToken<"nonlocal">,
    KeyToken<"while">,
    KeyToken<"assert">,
    KeyToken<"del">,
    KeyToken<"global">,
    KeyToken<"not">,
    KeyToken<"with">,
    KeyToken<"async">,
    KeyToken<"elif">,
    KeyToken<"if">,
    KeyToken<"or">,
    KeyToken<"yield">,

    // Literals
    StringToken,
    NumberToken,

    // Operators and Delimiters
    KeyToken<"-">,
    KeyToken<"-=">,
    KeyToken<"->">,
    KeyToken<",">,
    KeyToken<";">,
    KeyToken<":">,
    KeyToken<":=">,
    KeyToken<"!=">,
    KeyToken<".">,
    KeyToken<"(">,
    KeyToken<")">,
    KeyToken<"[">,
    KeyToken<"]">,
    KeyToken<"{">,
    KeyToken<"}">,
    KeyToken<"@">,
    KeyToken<"@=">,
    KeyToken<"*">,
    KeyToken<"**">,
    KeyToken<"**=">,
    KeyToken<"*=">,
    KeyToken<"/">,
    KeyToken<"//">,
    KeyToken<"//=">,
    KeyToken<"/=">,
    KeyToken<"&">,
    KeyToken<"&=">,
    KeyToken<"%">,
    KeyToken<"%=">,
    KeyToken<"^">,
    KeyToken<"^=">,
    KeyToken<"+">,
    KeyToken<"+=">,
    KeyToken<"<">,
    KeyToken<"<<">,
    KeyToken<"<<=">,
    KeyToken<"<=">,
    KeyToken<"=">,
    KeyToken<"==">,
    KeyToken<">">,
    KeyToken<">=">,
    KeyToken<">>">,
    KeyToken<">>=">,
    KeyToken<"|">,
    KeyToken<"|=">,
    KeyToken<"~">>;

class Lexer final {
  static auto keywords() {
    static auto *map = new std::map<std::string, Token, std::less<>>({
        {"False", Token(KeyToken<"False">{0})},
        {"await", Token(KeyToken<"await">{0})},
        {"else", Token(KeyToken<"else">{0})},
        {"import", Token(KeyToken<"import">{0})},
        {"pass", Token(KeyToken<"pass">{0})},
        {"None", Token(KeyToken<"None">{0})},
        {"break", Token(KeyToken<"break">{0})},
        {"except", Token(KeyToken<"except">{0})},
        {"in", Token(KeyToken<"in">{0})},
        {"raise", Token(KeyToken<"raise">{0})},
        {"True", Token(KeyToken<"True">{0})},
        {"class", Token(KeyToken<"class">{0})},
        {"finally", Token(KeyToken<"finally">{0})},
        {"is", Token(KeyToken<"is">{0})},
        {"return", Token(KeyToken<"return">{0})},
        {"and", Token(KeyToken<"and">{0})},
        {"continue", Token(KeyToken<"continue">{0})},
        {"for", Token(KeyToken<"for">{0})},
        {"lambda", Token(KeyToken<"lambda">{0})},
        {"try", Token(KeyToken<"try">{0})},
        {"as", Token(KeyToken<"as">{0})},
        {"def", Token(KeyToken<"def">{0})},
        {"from", Token(KeyToken<"from">{0})},
        {"nonlocal", Token(KeyToken<"nonlocal">{0})},
        {"while", Token(KeyToken<"while">{0})},
        {"assert", Token(KeyToken<"assert">{0})},
        {"del", Token(KeyToken<"del">{0})},
        {"global", Token(KeyToken<"global">{0})},
        {"not", Token(KeyToken<"not">{0})},
        {"with", Token(KeyToken<"with">{0})},
        {"async", Token(KeyToken<"async">{0})},
        {"elif", Token(KeyToken<"elif">{0})},
        {"if", Token(KeyToken<"if">{0})},
        {"or", Token(KeyToken<"or">{0})},
        {"yield", Token(KeyToken<"yield">{0})},
    });
    return &map;
  }
  static auto operators() {
    static auto *map = new std::map<std::string, Token, std::less<>>({
        {"-", Token(KeyToken<"-">{0})},
        {"-=", Token(KeyToken<"-=">{0})},
        {"->", Token(KeyToken<"->">{0})},
        {",", Token(KeyToken<",">{0})},
        {";", Token(KeyToken<";">{0})},
        {":", Token(KeyToken<":">{0})},
        {":=", Token(KeyToken<":=">{0})},
        {"!=", Token(KeyToken<"!=">{0})},
        {".", Token(KeyToken<".">{0})},
        {"(", Token(KeyToken<"(">{0})},
        {")", Token(KeyToken<")">{0})},
        {"[", Token(KeyToken<"[">{0})},
        {"]", Token(KeyToken<"]">{0})},
        {"{", Token(KeyToken<"{">{0})},
        {"}", Token(KeyToken<"}">{0})},
        {"@", Token(KeyToken<"@">{0})},
        {"@=", Token(KeyToken<"@=">{0})},
        {"*", Token(KeyToken<"*">{0})},
        {"**", Token(KeyToken<"**">{0})},
        {"**=", Token(KeyToken<"**=">{0})},
        {"*=", Token(KeyToken<"*=">{0})},
        {"/", Token(KeyToken<"/">{0})},
        {"//", Token(KeyToken<"//">{0})},
        {"//=", Token(KeyToken<"//=">{0})},
        {"/=", Token(KeyToken<"/=">{0})},
        {"&", Token(KeyToken<"&">{0})},
        {"&=", Token(KeyToken<"&=">{0})},
        {"%", Token(KeyToken<"%">{0})},
        {"%=", Token(KeyToken<"%=">{0})},
        {"^", Token(KeyToken<"^">{0})},
        {"^=", Token(KeyToken<"^=">{0})},
        {"+", Token(KeyToken<"+">{0})},
        {"+=", Token(KeyToken<"+=">{0})},
        {"<", Token(KeyToken<"<">{0})},
        {"<<", Token(KeyToken<"<<">{0})},
        {"<<=", Token(KeyToken<"<<=">{0})},
        {"<=", Token(KeyToken<"<=">{0})},
        {"=", Token(KeyToken<"=">{0})},
        {"==", Token(KeyToken<"==">{0})},
        {">", Token(KeyToken<">">{0})},
        {">=", Token(KeyToken<">=">{0})},
        {">>", Token(KeyToken<">>">{0})},
        {">>=", Token(KeyToken<">>=">{0})},
        {"|", Token(KeyToken<"|">{0})},
        {"|=", Token(KeyToken<"|=">{0})},
        {"~", Token(KeyToken<"~">{0})},
    });
    return &map;
  }

  const std::string source;

 public:
  Lexer(std::string &&s) : source(std::move(s)) {}
};

}  // namespace mtots
