#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>

// Number bases (also defined in Arduino.h, but needed here for defaults)
#ifndef DEC
#define DEC 10
#endif
#ifndef HEX
#define HEX 16
#endif
#ifndef OCT
#define OCT 8
#endif
#ifndef BIN
#define BIN 2
#endif

class Print {
public:
    virtual ~Print() = default;

    virtual size_t write(uint8_t c) = 0;

    virtual size_t write(const uint8_t *buffer, size_t size) {
        size_t n = 0;
        while (size--) {
            if (write(*buffer++)) n++;
            else break;
        }
        return n;
    }

    size_t write(const char *str) {
        if (str == nullptr) return 0;
        return write((const uint8_t *)str, strlen(str));
    }

    size_t write(const char *buffer, size_t size) {
        return write((const uint8_t *)buffer, size);
    }

    // Print methods
    size_t print(const char *str);
    size_t print(char c);
    size_t print(int n, int base = DEC);
    size_t print(unsigned int n, int base = DEC);
    size_t print(long n, int base = DEC);
    size_t print(unsigned long n, int base = DEC);
    size_t print(double n, int digits = 2);
    size_t print(const std::string& s);

    size_t println();
    size_t println(const char *str);
    size_t println(char c);
    size_t println(int n, int base = DEC);
    size_t println(unsigned int n, int base = DEC);
    size_t println(long n, int base = DEC);
    size_t println(unsigned long n, int base = DEC);
    size_t println(double n, int digits = 2);
    size_t println(const std::string& s);

    virtual void flush() {}

private:
    size_t printNumber(unsigned long n, uint8_t base);
};
