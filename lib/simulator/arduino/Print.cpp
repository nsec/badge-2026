#include "Print.h"
#include <cstdio>

size_t Print::print(const char *str) {
    if (str == nullptr) return 0;
    return write(str);
}

size_t Print::print(char c) {
    return write((uint8_t)c);
}

size_t Print::print(int n, int base) {
    return print((long)n, base);
}

size_t Print::print(unsigned int n, int base) {
    return print((unsigned long)n, base);
}

size_t Print::print(long n, int base) {
    if (base == 10 && n < 0) {
        size_t t = print('-');
        n = -n;
        return printNumber((unsigned long)n, base) + t;
    }
    return printNumber((unsigned long)n, base);
}

size_t Print::print(unsigned long n, int base) {
    return printNumber(n, base);
}

size_t Print::print(double n, int digits) {
    char buf[32];
    snprintf(buf, sizeof(buf), "%.*f", digits, n);
    return print(buf);
}

size_t Print::print(const std::string& s) {
    return print(s.c_str());
}

size_t Print::println() {
    return write('\r') + write('\n');
}

size_t Print::println(const char *str) {
    size_t n = print(str);
    return n + println();
}

size_t Print::println(char c) {
    size_t n = print(c);
    return n + println();
}

size_t Print::println(int num, int base) {
    size_t n = print(num, base);
    return n + println();
}

size_t Print::println(unsigned int num, int base) {
    size_t n = print(num, base);
    return n + println();
}

size_t Print::println(long num, int base) {
    size_t n = print(num, base);
    return n + println();
}

size_t Print::println(unsigned long num, int base) {
    size_t n = print(num, base);
    return n + println();
}

size_t Print::println(double num, int digits) {
    size_t n = print(num, digits);
    return n + println();
}

size_t Print::println(const std::string& s) {
    return println(s.c_str());
}

size_t Print::printNumber(unsigned long n, uint8_t base) {
    char buf[8 * sizeof(long) + 1];
    char *str = &buf[sizeof(buf) - 1];
    *str = '\0';

    if (base < 2) base = 10;

    do {
        unsigned long m = n;
        n /= base;
        char c = m - base * n;
        *--str = c < 10 ? c + '0' : c + 'A' - 10;
    } while (n);

    return write(str);
}
