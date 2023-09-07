#include <cstdint>
#include <cstdlib>
#include <functional>
#include <iostream>
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

[[noreturn]] inline void panic(std::function<void(std::ostream &)> f) {
  f(std::cerr);
  std::cerr << std::endl;
  abort();
}

[[noreturn]] inline void panic(std::string &&message) {
  std::cerr << message << std::endl;
  abort();
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
