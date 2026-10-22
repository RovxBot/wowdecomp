// Original 12340 language rules extracted from CSimpleRender.cpp.
#include "Engine/Source/Frame/CSimpleRender.h"
#include "Engine/Source/Base/ConvertUTF.h"
#include "Engine/Source/FrameScript/FrameScript.h"
#include "Engine/Source/Storm/H/storm.h"
#include <cctype>
#include <cstring>
#ifdef WOWDECOMP_PORTABLE_RUNTIME
#include "Engine/Source/Frame/Portable/Regions.h"
#include "Engine/Source/Os/Portable/Window.h"
#include <stdexcept>
#endif

// STATIC: 0x009EA254
static const uint8_t s_letterHasFinalConsonant[26] = { 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

// STATIC: 0x009EA270
static const uint8_t s_digitHasFinalConsonant[10] = { 1, 1, 0, 1, 0, 0, 1, 1, 1, 0 };

// STATIC: 0x00B41928
static uint16_t s_languageWideBuffer[0x2000];

// STATIC: 0x00B45928
static char s_languageBuffer[0x2000];

// VERIFY: d066960a manual
// OFFSET: 0x00481960
static const uint16_t* LanguageRule1ChooseParticle(uint32_t lastChar, const uint16_t* consonantForm, const uint16_t* vowelForm) {
    if (lastChar & 0xFF00) {
        if (static_cast<uint16_t>(lastChar + 0x5400) > 0x2BA3) {
            return consonantForm;
        }

        int32_t syllable = static_cast<uint16_t>(lastChar);

        if ((syllable - 0xAC08) % 28 == 0 && *consonantForm == 0xC73C) {
            return vowelForm;
        }

        return (syllable - 0xAC00) % 28 ? consonantForm : vowelForm;
    }

    if (static_cast<uint16_t>(lastChar - '0') <= 9) {
        return s_digitHasFinalConsonant[static_cast<uint16_t>(lastChar) - '0'] ? consonantForm : vowelForm;
    }

    if ((lastChar >= 'a' && lastChar <= 'z') || static_cast<uint16_t>(lastChar - 'A') <= 25) {
        return s_letterHasFinalConsonant[tolower(static_cast<uint16_t>(lastChar)) - 'a'] ? consonantForm : vowelForm;
    }

    return consonantForm;
}

// VERIFY: f67eabcd manual
// OFFSET: 0x00481A00
static char* LanguageRule1(const char* text) {
    uint16_t buffer[0x2000];
    uint32_t lastChar = 0;

    SUniConvertUTF8to16(buffer, sizeof(buffer) / sizeof(buffer[0]), text, STORM_MAX_STR, nullptr, nullptr);

    uint16_t* dest = s_languageWideBuffer;
    const uint16_t* src = buffer;

    if (buffer[0]) {
        do {
            uint16_t ch = *src;

            if (ch == '|') {
                src++;
                ch = *src;

                if (ch == '1' && (src[1] > '9' || src[1] < '0')) {
                    const uint16_t* consonantForm = src + 1;
                    const uint16_t* vowelForm = consonantForm;

                    if (*vowelForm) {
                        uint16_t c;

                        do {
                            c = *vowelForm;
                            vowelForm++;
                        } while (c != ';' && *vowelForm);
                    }

                    const uint16_t* end = vowelForm;

                    if (*end) {
                        uint16_t c;

                        do {
                            c = *end;
                            end++;
                        } while (c != ';' && *end);
                    }

                    src = end - 1;

                    const uint16_t* particle = LanguageRule1ChooseParticle(lastChar, consonantForm, vowelForm);

                    for (uint16_t c = *particle; c && c != ';'; c = *particle) {
                        particle++;
                        *dest = c;
                        dest++;
                    }
                } else {
                    *dest = '|';
                    dest++;
                    *dest = ch;
                    dest++;

                    if (ch == 'c' || ch == 'C') {
                        for (uint32_t i = 0; i < 8 && src[1]; i++) {
                            src++;
                            *dest = *src;
                            dest++;
                        }
                    }
                }
            } else {
                if ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') || static_cast<uint16_t>(ch + 0x5400) <= 0x2BA3) {
                    lastChar = ch;
                }

                *dest = ch;
                dest++;
            }

            src++;
        } while (*src);
    }

    *dest = 0;
    SUniConvertUTF16to8(s_languageBuffer, sizeof(s_languageBuffer), s_languageWideBuffer, STORM_MAX_STR, nullptr, nullptr);

    return s_languageBuffer;
}

// VERIFY: 8cd26535 manual
// OFFSET: 0x00481BC0
static bool LanguageRule2IsVowel(uint16_t ch) {
    switch (ch) {
        case 'A':
        case 'E':
        case 'I':
        case 'O':
        case 'U':
        case 'a':
        case 'e':
        case 'i':
        case 'o':
        case 'u':
            return true;

        default:
            return (ch >= 0xC0 && ch <= 0xC6) || (ch >= 0xC8 && ch <= 0xCF) || (ch >= 0xD2 && ch <= 0xD6) || (ch >= 0xD8 && ch <= 0xDC) || (ch >= 0xE0 && ch <= 0xE6) || (ch >= 0xE8 && ch <= 0xEF) || (ch >= 0xF2 && ch <= 0xF6) || static_cast<uint16_t>(ch - 0xF8) <= 4;
    }
}

// VERIFY: 7f584b59 manual
// OFFSET: 0x00481C80
static char* LanguageRule2(const char* text) {
    uint16_t buffer[0x2000];

    SUniConvertUTF8to16(buffer, sizeof(buffer) / sizeof(buffer[0]), text, STORM_MAX_STR, nullptr, nullptr);

    uint16_t* dest = s_languageWideBuffer;
    const uint16_t* src = buffer;

    if (buffer[0]) {
        do {
            if (*src == '|') {
                src++;

                if (*src == '2' && (src[1] > '9' || src[1] < '0')) {
                    src++;

                    while (*src == ' ') {
                        src++;
                    }

                    uint16_t ch = *src;

                    if (ch == 'L' && src[1] == '\'') {
                        *dest++ = 'd';
                        *dest++ = 'e';
                        *dest++ = ' ';
                        *dest++ = 'l';
                        *dest = '\'';
                        src += 2;
                    } else if (ch == 'L' && src[1] == 'e' && src[2] == ' ') {
                        *dest++ = 'd';
                        *dest++ = 'u';
                        *dest = ' ';
                        src += 3;
                    } else if (ch == 'L' && src[1] == 'a' && src[2] == ' ') {
                        *dest++ = 'd';
                        *dest++ = 'e';
                        *dest++ = ' ';
                        *dest++ = 'l';
                        *dest++ = 'a';
                        *dest = ' ';
                        src += 3;
                    } else if (ch == 'L' && src[1] == 'e' && src[2] == 's' && src[3] == ' ') {
                        *dest++ = 'd';
                        *dest++ = 'e';
                        *dest++ = 's';
                        *dest = ' ';
                        src += 4;
                    } else if (LanguageRule2IsVowel(ch) || ch == 'h' || ch == 'H' || (ch == 'y' && LanguageRule2IsVowel(src[1])) || (ch == 'Y' && LanguageRule2IsVowel(src[1]))) {
                        *dest++ = 'd';
                        *dest = '\'';
                    } else {
                        *dest++ = 'd';
                        *dest++ = 'e';
                        *dest = ' ';
                    }
                } else {
                    *dest = '|';
                }

                dest++;
            }

            *dest = *src;
            src++;
            dest++;
        } while (*src);
    }

    *dest = 0;
    SUniConvertUTF16to8(s_languageBuffer, sizeof(s_languageBuffer), s_languageWideBuffer, STORM_MAX_STR, nullptr, nullptr);

    return s_languageBuffer;
}

#ifndef WOWDECOMP_PORTABLE_RUNTIME
// VERIFY: 1b50c04d manual
// OFFSET: 0x00481E90
static char* LanguageRule3(const char* text) {
    char copy[0x2000];
    char word[0x400];

    if (text == s_languageBuffer) {
        SStrCopy(copy, text, sizeof(copy));
        text = copy;
    }

    const char* src = text;
    char* dest = s_languageBuffer;

    while (*src) {
        if (*src == '|') {
            src++;

            if (*src == '3' && (src[1] > '9' || src[1] < '0')) {
                if (src[1] == '-' && src[2] >= '0' && src[2] <= '9') {
                    src += 2;
                }

                uint32_t declension = SStrToUnsigned(&src);

                if (*src == '(') {
                    src++;
                }

                char* wordEnd = word;

                while (*src && *src != ')') {
                    if (wordEnd < &word[sizeof(word) - 1]) {
                        *wordEnd++ = *src;
                    }

                    src++;
                }

                if (!*src) {
                    src--;
                }

                *wordEnd = 0;
                dest += GetDeclinedWord(word, declension, dest, &s_languageBuffer[sizeof(s_languageBuffer)] - dest);
            } else {
                *dest = '|';
                dest++;
                *dest = *src;
                dest++;
            }
        } else {
            *dest = *src;
            dest++;
        }

        src++;
    }

    *dest = 0;

    return s_languageBuffer;
}

#endif

// VERIFY: 1e0898ef manual
// OFFSET: 0x00481FC0
static char* LanguageRule4(const char* text) {
    bool wasDigit = false;
    int32_t number = 0;
    const char* src = text;
    char* dest = s_languageBuffer;

    while (*src) {
        char ch = *src;
        bool isDigit;

        if (ch >= '0' && ch <= '9') {
            number = wasDigit ? number * 10 + ch - '0' : ch - '0';
            isDigit = true;
        } else {
            isDigit = false;
        }

        if (ch == '|') {
            src++;

            if (*src == '4' && (src[1] > '9' || src[1] < '0')) {
                src++;

                uint32_t index = FrameScript_GetPluralIndex(number);

                for (uint32_t i = 0; i < index && *src; i++) {
                    while (*src != ':' && *src != ';') {
                        src++;

                        if (!*src) {
                            break;
                        }
                    }

                    if (!*src || *src == ';') {
                        break;
                    }

                    src++;
                }

                while (*src == ' ') {
                    src++;
                }

                if (!*src) {
                    break;
                }

                while (*src != ':' && *src != ';') {
                    *dest = *src;
                    src++;
                    dest++;

                    if (!*src) {
                        *dest = 0;
                        return s_languageBuffer;
                    }
                }

                while (*src != ';') {
                    src++;

                    if (!*src) {
                        *dest = 0;
                        return s_languageBuffer;
                    }
                }
            } else {
                *dest = '|';
                dest++;
                *dest = *src;
                dest++;
            }
        } else {
            *dest = ch;
            dest++;
        }

        src++;
        wasDigit = isDigit;
    }

    *dest = 0;

    return s_languageBuffer;
}

#ifndef WOWDECOMP_PORTABLE_RUNTIME
// VERIFY: 44de3423 manual
// OFFSET: 0x00482110
const char* LanguageProcess(const char* text) {
    if (SStrLen(text) > sizeof(s_languageBuffer)) {
        memcpy(s_languageBuffer, text, sizeof(s_languageBuffer));
        s_languageBuffer[sizeof(s_languageBuffer) - 3] = 0;
        s_languageBuffer[sizeof(s_languageBuffer) - 2] = 0;
        s_languageBuffer[sizeof(s_languageBuffer) - 1] = 0;
        text = s_languageBuffer;
    }

    for (;;) {
        const uint8_t* src = reinterpret_cast<const uint8_t*>(text);
        int32_t chars;
        uint32_t ch;
        int32_t rule = 0;

        while ((ch = sgetu8(src, &chars)) != 0xFFFFFFFF) {
            src += chars;

            if (ch != '|') {
                continue;
            }

            if (*src == '|') {
                src++;
                continue;
            }

            if (*src >= '0' && *src <= '9') {
                rule = SStrToInt(reinterpret_cast<const char*>(src));

                if (rule >= 1 && rule <= 4) {
                    break;
                }
            }
        }

        if (ch == 0xFFFFFFFF) {
            return text;
        }

        switch (rule) {
            case 1:
                text = LanguageRule1(text);
                break;

            case 2:
                text = LanguageRule2(text);
                break;

            case 3:
                text = LanguageRule3(text);
                break;

            case 4:
                text = LanguageRule4(text);
                break;
        }
    }
}

#else
// Native admission around the original rules. Rule 3 needs game DB/name caches.
namespace {
int ValidateLanguage(const char* text) {
    if (!text) throw std::invalid_argument("Null native language input");
    const size_t length = std::strlen(text);
    if (length > 4095) throw std::length_error("Native language input exceeds 4095 bytes");
    OsNative::DecodeUtf8(text);
    int firstRule = 0;
    for (size_t i = 0; i < length; ++i) {
        if (text[i] != '|') continue;
        if (++i == length) throw std::invalid_argument("Incomplete language escape");
        if (text[i] == '|') continue;
        const char code = text[i];
        if (code < '1' || code > '4' || (text[i+1] >= '0' && text[i+1] <= '9')) continue;
        if (code == '3') throw std::logic_error("Native declension requires game DB/name caches");
        if (!firstRule) firstRule = code - '0';
        if (code == '1') {
            const char* middle = std::strchr(text+i+1, ';');
            if (!middle || !std::strchr(middle+1, ';'))
                throw std::invalid_argument("Incomplete particle alternatives");
        } else if (code == '2') {
            size_t next = i+1;
            while (text[next] == ' ') ++next;
            if (!text[next]) throw std::invalid_argument("Missing article word");
        } else if (!std::strchr(text+i+1, ';')) {
            throw std::invalid_argument("Unterminated plural alternatives");
        }
    }
    if (firstRule) {
        // Rule 4 accumulates every decimal run into an original signed counter.
        // Bound before calling any rule, including after previous rule output.
        uint32_t number = 0;
        for (size_t i = 0; i < length; ++i) {
            if (text[i] < '0' || text[i] > '9') { number = 0; continue; }
            const uint32_t digit = text[i] - '0';
            if (number > (uint32_t(INT32_MAX)-digit)/10)
                throw std::out_of_range("Native language decimal counter overflow");
            number = number*10+digit;
        }
    }
    return firstRule;
}
}
const char* LanguageProcess(const char* text) {
    Frame::Native::RequireRegions();
    // Revalidate every intermediate output: escaped/selected text may expose
    // another rule. Bound progress without changing the original rule bodies.
    for (size_t pass = 0; pass < 64; ++pass) {
        switch (ValidateLanguage(text)) {
            case 0: return text;
            case 1: text = LanguageRule1(text); break;
            case 2: text = LanguageRule2(text); break;
            case 4: text = LanguageRule4(text); break;
        }
    }
    throw std::length_error("Native language pass limit");
}
#endif
