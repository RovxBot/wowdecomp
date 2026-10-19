#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#if defined(WOWDECOMP_PORTABLE_RUNTIME)
#include <climits>
#include <algorithm>
#endif
#include "Engine/Source/Storm/H/storm.h"

// Native ABI adapters below do not inherit the original Windows VERIFY claims.

#define MAX_ARGUMENTS 64
#define MAX_SPECIFIER_LENGTH 64
#define UNBOUNDED_OUTPUT_SIZE 0x100000

struct FormatSpecifier {
    /* 0000 */ const char* start;
    /* 0004 */ uint32_t length;
    /* 0008 */ int32_t argument;
};

union ArgumentValue {
    /* 0000 */ int32_t intValue;
    /* 0000 */ void* pointerValue;
    /* 0000 */ int64_t longLongValue;
    /* 0000 */ double doubleValue;
};

// VERIFY: 291dacd5 manual
// OFFSET: 0x00773CC0
static int32_t ParseFormatSpecifier(const char** format, ArgumentSize* size, int32_t* orderingPtr) {
    const char* curr = *format;
    int32_t currentNumber = 0;
    *size = e_intSized;
#if defined(WOWDECOMP_PORTABLE_RUNTIME)
    bool lengthSet = false;
#endif

    for (;;) {
#if defined(WOWDECOMP_PORTABLE_RUNTIME)
        if (curr - *format >= MAX_SPECIFIER_LENGTH - 1) return -1;
#endif
        char ch = *curr++;

        if (ch >= '0' && ch <= '9') {
#if defined(WOWDECOMP_PORTABLE_RUNTIME)
            if (currentNumber > (INT_MAX - (ch - '0')) / 10) return -1;
#endif
            currentNumber = 10 * currentNumber + ch - '0';
            continue;
        }

        if (ch == '$') {
            if (currentNumber <= 0) {
                return -1;
            }

            *orderingPtr = currentNumber - 1;
            continue;
        }

        currentNumber = 0;
        int32_t result;

        switch (ch) {
        case ' ':
        case '#':
        case '+':
        case '-':
        case '.':
            continue;
        case '%':
#if defined(WOWDECOMP_PORTABLE_RUNTIME)
            if (lengthSet) return -1;
#endif
            *size = e_takesNoSpace;
            result = 0;
            break;
        case 'C':
#if defined(WOWDECOMP_PORTABLE_RUNTIME)
            // The original means a UTF-16 wide character, not host wchar_t.
            return -1;
#endif
        case 'D':
        case 'O':
        case 'U':
        case 'X':
        case 'c':
        case 'd':
        case 'i':
        case 'o':
        case 'u':
        case 'x':
#if defined(WOWDECOMP_PORTABLE_RUNTIME)
            if (ch == 'c' && lengthSet) return -1;
#endif
            result = 1;
            break;
        case 'E':
        case 'F':
        case 'G':
        case 'e':
        case 'f':
        case 'g':
#if defined(WOWDECOMP_PORTABLE_RUNTIME)
            if (lengthSet) return -1;
#endif
            *size = e_doubleSized;
            result = 1;
            break;
        case 'I':
#if defined(WOWDECOMP_PORTABLE_RUNTIME)
            if (lengthSet) return -1;
            lengthSet = true;
#endif
            if (curr[0] != '6' || curr[1] != '4') {
                return -1;
            }

            curr += 2;
            *size = e_longLongSized;
            continue;
#if !defined(WOWDECOMP_PORTABLE_RUNTIME)
        case 'P':
#endif
        case 'S':
#if defined(WOWDECOMP_PORTABLE_RUNTIME)
            // UTF-16 wide-string formatting needs its own encoding adapter.
            return -1;
        case 'P':
#endif
        case 'p':
        case 's':
#if defined(WOWDECOMP_PORTABLE_RUNTIME)
            if (lengthSet) return -1;
#endif
            *size = e_pointerSized;
            result = 1;
            break;
        case 'l':
#if defined(WOWDECOMP_PORTABLE_RUNTIME)
            if (lengthSet) return -1;
            lengthSet = true;
#endif
            if (*curr != 'l') {
                return -1;
            }

            curr++;
            *size = e_longLongSized;
            continue;
        default:
            return -1;
        }

        if (curr - *format + 1 >= MAX_SPECIFIER_LENGTH) {
            return -1;
        }

        *format = curr;
        return result;
    }
}

// VERIFY: 7c8d9894 manual
// OFFSET: 0x00773E10
static void FixUpLongLongFormatSpecifier(char* specifier) {
#if defined(WOWDECOMP_PORTABLE_RUNTIME)
    char* legacy = strstr(specifier, "I64");
    if (legacy) {
        memcpy(legacy, "ll", 2);
        memmove(legacy + 2, legacy + 3, strlen(legacy + 3) + 1);
    }
#else
    char* ll = strstr(specifier, "ll");

    if (ll) {
        memmove(ll + 3, ll + 2, strlen(ll) - 1);
        memcpy(ll, "I64", 3);
    }
#endif
}

// VERIFY: 33f0aefe manual
// OFFSET: 0x00773E60
int32_t vsnoprintf(char* out, size_t outSize, const char* format, va_list argumentList) {
#if defined(WOWDECOMP_PORTABLE_RUNTIME)
    if (!out || !format || outSize > INT_MAX) {
        SErrSetLastError(STORM_ERROR_INVALID_PARAMETER);
        return 0;
    }
#endif
    if (!outSize) {
        return 0;
    }

    char* dest = out;
    char* end = &out[outSize - 1];

    ArgumentSize sizes[MAX_ARGUMENTS + 1] = {};
    ArgumentValue values[MAX_ARGUMENTS] = {};
    FormatSpecifier specifiers[MAX_ARGUMENTS] = {};
    sizes[MAX_ARGUMENTS] = e_takesNoSpace;

    int32_t orderingPtr = 0;
    int32_t numArguments = 0;
    int32_t numSpecifiers = 0;
    FormatSpecifier* spec = specifiers;
    const char* curr = format;
    char ch;

    do {
        ch = *curr++;

        if (ch != '%') {
            continue;
        }

        if (numSpecifiers >= MAX_ARGUMENTS) {
#if defined(WOWDECOMP_PORTABLE_RUNTIME)
            SErrSetLastError(STORM_ERROR_INVALID_PARAMETER);
#endif
            *dest = 0;
            return dest - out;
        }

        const char* start = curr - 1;
        ArgumentSize size;
        int32_t result = ParseFormatSpecifier(&curr, &size, &orderingPtr);

        if (result == -1) {
#if defined(WOWDECOMP_PORTABLE_RUNTIME)
            SErrSetLastError(STORM_ERROR_INVALID_PARAMETER);
#endif
            *dest = 0;
            return dest - out;
        }

        spec->start = start;
        spec->length = curr - start;

        if (result > 0) {
#if defined(WOWDECOMP_PORTABLE_RUNTIME)
            if (orderingPtr < 0 || orderingPtr >= MAX_ARGUMENTS) {
                *dest = 0;
                SErrSetLastError(STORM_ERROR_INVALID_PARAMETER);
                return 0;
            }
#endif
            if (sizes[orderingPtr] && sizes[orderingPtr] != size) {
#if defined(WOWDECOMP_PORTABLE_RUNTIME)
                SErrSetLastError(STORM_ERROR_INVALID_PARAMETER);
#endif
                *dest = 0;
                return dest - out;
            }

            spec->argument = orderingPtr;
            sizes[orderingPtr] = size;
            orderingPtr++;

            if (numArguments < orderingPtr) {
                numArguments = orderingPtr;
            }
        } else {
            spec->argument = MAX_ARGUMENTS;
        }

        numSpecifiers++;
        spec++;
    } while (ch);

#if defined(WOWDECOMP_PORTABLE_RUNTIME)
    // va_list is an array on some native ABIs and a pointer on others.
    // Consume a copy so the caller retains ownership and can reuse its list.
    va_list nativeArguments;
    va_copy(nativeArguments, argumentList);
#define WOW_FORMAT_ARGUMENTS nativeArguments
#else
#define WOW_FORMAT_ARGUMENTS argumentList
#endif
    for (int32_t i = 0; i < numArguments; i++) {
        switch (sizes[i]) {
        case e_unspecified:
#if defined(WOWDECOMP_PORTABLE_RUNTIME)
            va_end(nativeArguments);
            SErrSetLastError(STORM_ERROR_INVALID_PARAMETER);
#endif
            *dest = 0;
            return dest - out;
        case e_intSized:
            values[i].intValue = va_arg(WOW_FORMAT_ARGUMENTS, int32_t);
            break;
        case e_pointerSized:
            values[i].pointerValue = va_arg(WOW_FORMAT_ARGUMENTS, void*);
            break;
        case e_longLongSized:
            values[i].longLongValue = va_arg(WOW_FORMAT_ARGUMENTS, int64_t);
            break;
        case e_doubleSized:
            values[i].doubleValue = va_arg(WOW_FORMAT_ARGUMENTS, double);
            break;
        default:
            values[i].doubleValue = 0.0;
            break;
        }
    }
#if defined(WOWDECOMP_PORTABLE_RUNTIME)
    va_end(nativeArguments);
#endif
#undef WOW_FORMAT_ARGUMENTS

    spec = specifiers;

    while (dest < end) {
        ch = *format++;

        if (!ch) {
            break;
        }

        if (ch != '%') {
            *dest++ = ch;
            continue;
        }

        char buffer[MAX_SPECIFIER_LENGTH];
        uint32_t length = spec->length;
        memcpy(buffer, spec->start, length);
        buffer[length] = 0;

        char* specifier = strchr(buffer, '$');

        if (specifier) {
            *specifier = '%';
        } else {
            specifier = buffer;
        }

        int32_t argument = spec->argument;
        spec++;

        int32_t avail = static_cast<int32_t>(end - dest);
        int32_t written = 0;

#if defined(WOWDECOMP_PORTABLE_RUNTIME)
        FixUpLongLongFormatSpecifier(specifier);
        char& conversion = specifier[strlen(specifier) - 1];
        if (conversion == 'D') conversion = 'd';
        if (conversion == 'O') conversion = 'o';
        if (conversion == 'U') conversion = 'u';
        if (conversion == 'P') conversion = 'p';
        const bool unsignedInteger = conversion == 'u' || conversion == 'o' || conversion == 'x' || conversion == 'X';
        switch (sizes[argument]) {
        case e_intSized:
            written = unsignedInteger ? snprintf(dest, size_t(avail) + 1, specifier, static_cast<uint32_t>(values[argument].intValue))
                                      : snprintf(dest, size_t(avail) + 1, specifier, values[argument].intValue);
            break;
        case e_pointerSized:
            written = snprintf(dest, size_t(avail) + 1, specifier, values[argument].pointerValue);
            break;
        case e_longLongSized:
            written = unsignedInteger ? snprintf(dest, size_t(avail) + 1, specifier, static_cast<unsigned long long>(values[argument].longLongValue))
                                      : snprintf(dest, size_t(avail) + 1, specifier, static_cast<long long>(values[argument].longLongValue));
            break;
        case e_doubleSized:
            written = snprintf(dest, size_t(avail) + 1, specifier, values[argument].doubleValue);
            break;
        case e_takesNoSpace:
            written = snprintf(dest, size_t(avail) + 1, specifier);
            break;
        }
#else
        switch (sizes[argument]) {
        case e_intSized:
            written = _snprintf(dest, avail, specifier, values[argument].intValue);
            break;
        case e_pointerSized:
            written = _snprintf(dest, avail, specifier, values[argument].pointerValue);
            break;
        case e_longLongSized:
            FixUpLongLongFormatSpecifier(specifier);
            written = _snprintf(dest, avail, specifier, values[argument].longLongValue);
            break;
        case e_doubleSized:
            written = _snprintf(dest, avail, specifier, values[argument].doubleValue);
            break;
        case e_takesNoSpace:
            written = _snprintf(dest, avail, specifier);
            break;
        }
#endif

        if (written < 0 || written > avail) {
            written = avail;
        }

        dest += written;
        format += length - 1;
    }

    *dest = 0;
    return dest - out;
}

// VERIFY: 04457973 auto
// OFFSET: 0x007741B0
int32_t vsoprintf(char* out, const char* format, va_list argumentList) {
    return vsnoprintf(out, UNBOUNDED_OUTPUT_SIZE, format, argumentList);
}
