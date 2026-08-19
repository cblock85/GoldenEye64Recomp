#include "patches.h"

f32 __floatundisf(u32 value) {
    if (value == 0) {
        return 0.0f;
    }

    int shift = 0;

    // Normalize the value (shift left until highest bit is at 31)
    while ((value & 0x80000000) == 0) {
        value <<= 1;
        shift++;
    }

    // Build float manually
    // IEEE 754 float: 1 sign bit | 8 exponent bits | 23 mantissa bits
    // Exponent bias for float is 127

    u32 exponent = 158 - shift; // 127 (bias) + 31 (bit position) - shift
    u32 mantissa = (value >> 8) & 0x007FFFFF; // keep only 23 bits after leading 1

    u32 resultBits = (exponent << 23) | mantissa;

    f32 result;
    *((u32*)&result) = resultBits; // Type punning: treat bits as float

    return result;
}

u64 __fixunssfdi(f32 value) {
    if (value <= 0.0f) {
        return 0;
    }

    u32 bits = *((u32*)&value); // Interpret float bits as u32

    u32 exponent = (bits >> 23) & 0xFF;
    u32 mantissa = bits & 0x7FFFFF; // 23 bits mantissa

    if (exponent == 0) {
        // Denormalized number (very close to zero)
        return 0;
    }

    exponent -= 127; // remove bias

    if (exponent >= 64) {
        // Overflow: larger than u64 can hold
        return (u64)-1;
    }

    // Add implicit 1 before mantissa
    u64 int_part = (1ULL << 23) | mantissa;

    if (exponent > 23) {
        int_part <<= (exponent - 23);
    } else {
        int_part >>= (23 - exponent);
    }

    return int_part;
}