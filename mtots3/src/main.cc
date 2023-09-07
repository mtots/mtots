#include "mtots.hh"

using namespace mtots;
using namespace std;

int main() {
  auto str = String::make("Hello world!");
  cout << str << endl;

  auto list = List::make({1.0, 2.0, 3.0});
  cout << list << endl;

  auto dict = Dict::make({
      {1.0, 3.0},
      {String::make("key"), String::make("value")},
      {intern("key"), String::make("value")},
  });
  cout << "dict = " << dict << endl;

  panic([](auto &out) {
    out << "There was some error";
  });
}
