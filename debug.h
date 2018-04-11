#ifndef DEBUG_H_
#define DEBUG_H_

template<class Param>
class Hex {
public:
   Param value;
   Hex(Param value): value(value) {}
};

#ifndef DEBUG

template<class... Params>
void debug(Params...) {}

template<class... Params>
void debugln(Params...) {}

#else

template<class Param>
struct Output {
  static void output(Param arg) {
    Serial.print(arg);
  }
};

template<class Param>
struct Output<Hex<Param>> {
  static void output(Hex<Param> arg) {
    Serial.print(arg.value, HEX);
  }
};

template<class... Params>
struct Debug {};

template<>
struct Debug<> {
  static void output() {}
};

template<class First, class... Params>
struct Debug<First, Params...> {
  static void output(First first, Params... args) {
    Output<First>::output(first);
    Debug<Params...>::output(args...);
  }
};

template<class... Params>
void debug(Params... args) {
  Debug<Params...>::output(args...);
}

template<class... Params>
void debugln(Params... args) {
  debug<Params...>(args...);
  Serial.println();
}

#endif

#endif
