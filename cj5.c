// TODO:
//      utf8 support for strings
//      counter should not care for max_tokens
//      better error handling
//      hash name optimization ?
//      writer? (do we need it?)
//      DOM
#pragma once

#include <stdbool.h>    // bool
#include <stdint.h>     // uint32_t, int64_t, etc.

#ifndef CJ5_ASSERT
#    include <assert.h>
#    define CJ5_ASSERT(_e) assert(_e)
#endif

#ifndef CJ5_MEMCPY
#    include <string.h>    // memcpy
#    define CJ5_MEMCPY(_dst, _src, _n) memcpy((_dst), (_src), (_n))
#endif


typedef enum cj5_token_type {
    CJ5_TOKEN_OBJECT = 0,
    CJ5_TOKEN_ARRAY,
    CJ5_TOKEN_NUMBER,
    CJ5_TOKEN_STRING,
    CJ5_TOKEN_BOOL,
    CJ5_TOKEN_NULL
} cj5_token_type;

typedef enum cj5_token_number_type {
    CJ5_TOKEN_NUMBER_UNKNOWN = 0,
    CJ5_TOKEN_NUMBER_FLOAT,
    CJ5_TOKEN_NUMBER_INT,
    CJ5_TOKEN_NUMBER_HEX
} cj5_token_number_type;

typedef struct cj5_token {
    cj5_token_type type;
    union {
        cj5_token_number_type num_type;
        uint32_t key_hash;
    };
    int start;
    int end;
    int size;
    int parent_id;
} cj5_token;

typedef struct cj5_result {
    bool error;
    int error_line;
    int error_pos;
    int num_tokens;
    const cj5_token* tokens;
    const char* json5;
} cj5_result;

////////////////////////////////////////////////////////////////////////////////////////////////////
// private
static inline bool cj5__isspace(char ch)
{
    return (uint32_t)(ch - 1) < 32 && ((0x80001F00 >> (uint32_t)(ch - 1)) & 1) == 1;
}

static inline bool cj5__isrange(char ch, char from, char to)
{
    return (uint8_t)(ch - from) <= (uint8_t)(to - from);
}

static inline bool cj5__isupperchar(char ch)
{
    return cj5__isrange(ch, 'A', 'Z');
}

static inline bool cj5__islowerchar(char ch)
{
    return cj5__isrange(ch, 'a', 'z');
}

static inline bool cj5__isnum(char ch)
{
    return cj5__isrange(ch, '0', '9');
}

#define CJ5__ARCH_64BIT 0
#define CJ5__ARCH_32BIT 0
#if defined(__x86_64__) || defined(_M_X64) || defined(__aarch64__) || defined(__64BIT__) || \
    defined(__mips64) || defined(__powerpc64__) || defined(__ppc64__) || defined(__LP64__)
#    undef CJ5__ARCH_64BIT
#    define CJ5__ARCH_64BIT 64
#else
#    undef CJ5__ARCH_32BIT
#    define CJ5__ARCH_32BIT 32
#endif    //

#if defined(_MSC_VER)
#    define CJ5__RESTRICT __restrict
#else
#    define CJ5__RESTRICT __restrict__
#endif

#define CJ5__UNUSED(_a) (void)(_a)

#define CJ5__FOURCC(_a, _b, _c, _d) \
    (((uint32_t)(_a) | ((uint32_t)(_b) << 8) | ((uint32_t)(_c) << 16) | ((uint32_t)(_d) << 24)))

static const uint32_t CJ5__NULL_FOURCC = CJ5__FOURCC('n', 'u', 'l', 'l');
static const uint32_t CJ5__TRUE_FOURCC = CJ5__FOURCC('t', 'r', 'u', 'e');
static const uint32_t CJ5__FALSE_FOURCC = CJ5__FOURCC('f', 'a', 'l', 's');

#define CJ5__FNV1_32_INIT 0x811c9dc5
#define CJ5__FNV1_32_PRIME 0x01000193
static inline uint32_t cj5__hash_fnv32(const char* start, const char* end)
{
    const char* bp = start;
    const char* be = end;    // start + len

    uint32_t hval = CJ5__FNV1_32_INIT;
    while (bp < be) {
        hval ^= (uint32_t)*bp++;
        hval *= CJ5__FNV1_32_PRIME;
    }

    return hval;
}

// https://github.com/lattera/glibc/blob/master/string/strlen.c
static int cj5__strlen(const char* str)
{
    const char* char_ptr;
    const uintptr_t* longword_ptr;
    uintptr_t longword, himagic, lomagic;

    for (char_ptr = str; ((uintptr_t)char_ptr & (sizeof(longword) - 1)) != 0; ++char_ptr) {
        if (*char_ptr == '\0')
            return (int)(intptr_t)(char_ptr - str);
    }
    longword_ptr = (uintptr_t*)char_ptr;
    himagic = 0x80808080L;
    lomagic = 0x01010101L;
#if CJ5__ARCH_64BIT
    /* 64-bit version of the magic.  */
    /* Do the shift in two steps to avoid a warning if long has 32 bits.  */
    himagic = ((himagic << 16) << 16) | himagic;
    lomagic = ((lomagic << 16) << 16) | lomagic;
#endif

    for (;;) {
        longword = *longword_ptr++;

        if (((longword - lomagic) & ~longword & himagic) != 0) {
            const char* cp = (const char*)(longword_ptr - 1);

            if (cp[0] == 0)
                return (int)(intptr_t)(cp - str);
            if (cp[1] == 0)
                return (int)(intptr_t)(cp - str + 1);
            if (cp[2] == 0)
                return (int)(intptr_t)(cp - str + 2);
            if (cp[3] == 0)
                return (int)(intptr_t)(cp - str + 3);
#if CJ5__ARCH_64BIT
            if (cp[4] == 0)
                return (int)(intptr_t)(cp - str + 4);
            if (cp[5] == 0)
                return (int)(intptr_t)(cp - str + 5);
            if (cp[6] == 0)
                return (int)(intptr_t)(cp - str + 6);
            if (cp[7] == 0)
                return (int)(intptr_t)(cp - str + 7);
#endif
        }
    }

    CJ5_ASSERT(0 && "Not a null-terminated string");
    return -1;
}

typedef struct cj5__parser {
    int pos;
    int next_id;
    int super_id;
} cj5__parser;

static cj5_token* cj5__alloc_token(cj5__parser* parser, cj5_token* tokens, int max_tokens)
{
    if (parser->next_id >= max_tokens) {
        return NULL;
    }

    cj5_token* token = &tokens[parser->next_id++];
    *token = (cj5_token){ .start = -1, .end = -1, .parent_id = -1 };
    return token;
}

static cj5_result cj5__parse_primitive(cj5__parser* parser, const char* json5, int len,
                                       cj5_token* tokens, int max_tokens)
{
    cj5_token* token;
    int start = parser->pos;
    bool keyname = false;

    for (; parser->pos < len; parser->pos++) {
        switch (json5[parser->pos]) {
        case ':':
            keyname = true;
        case '\t':
        case '\r':
        case '\n':
        case ' ':
        case ',':
        case ']':
        case '}':
            goto found;
        default:
            break;
        }

        if (json5[parser->pos] < 32 || json5[parser->pos] >= 127) {
            parser->pos = start;
            return (cj5_result){ .error = true };    // invalid
        }
    }

    parser->pos = start;
    return (cj5_result){ .error = true };    // incomplete

found:
    if (tokens == NULL) {
        --parser->pos;
        return (cj5_result){ 0 };
    }
    token = cj5__alloc_token(parser, tokens, max_tokens);
    if (token == NULL) {
        parser->pos = start;
        return (cj5_result){ .error = true };    // out of memory
    }

    cj5_token_type type;
    cj5_token_number_type num_type = 0;
    if (keyname) {
        // JSON5: it is likely a key-name, validate and interpret as string
        for (int i = start; i < parser->pos; i++) {
            if (cj5__islowerchar(json5[i]) || cj5__isupperchar(json5[i]) || json5[i] == '_') {
                continue;
            }

            if (cj5__isnum(json5[i])) {
                if (i == start) {
                    parser->pos = start;
                    return (cj5_result){ .error = true };    // invalid
                }
                continue;
            }

            parser->pos = start;
            return (cj5_result){ .error = true };    // invalid
        }

        type = CJ5_TOKEN_STRING;
    } else {
        // detect other types, subtypes
        uint32_t* fourcc = (uint32_t*)&json5[start];
        if (*fourcc == CJ5__NULL_FOURCC) {
            type = CJ5_TOKEN_NULL;
        } else if (*fourcc == CJ5__TRUE_FOURCC) {
            type = CJ5_TOKEN_BOOL;
        } else if (*fourcc == CJ5__FALSE_FOURCC) {
            type = CJ5_TOKEN_BOOL;
        } else {
            num_type = CJ5_TOKEN_NUMBER_INT;
            // hex number
            if (json5[start] == '0' && start < parser->pos + 1 && json5[start + 1] == 'x') {
                start = start + 2;
                for (int i = start; i < parser->pos; i++) {
                    if (!(cj5__isrange(json5[i], '0', '9') || cj5__isrange(json5[i], 'A', 'F') ||
                          cj5__isrange(json5[i], 'a', 'f'))) {
                        parser->pos = start;
                        return (cj5_result){ .error = true };    // invalid
                    }
                }
                num_type = CJ5_TOKEN_NUMBER_HEX;
            } else {
                int start_index = start;
                if (json5[start] == '+') {
                    ++start_index;
                    ++start;
                } else if (json5[start] == '-') {
                    ++start_index;
                }

                for (int i = start_index; i < parser->pos; i++) {
                    if (json5[i] == '.') {
                        if (num_type == CJ5_TOKEN_NUMBER_FLOAT) {
                            return (cj5_result){ .error = true };    // invalid
                        }
                        num_type = CJ5_TOKEN_NUMBER_FLOAT;
                    }

                    if (!cj5__isnum(json5[i])) {
                        parser->pos = start;
                        return (cj5_result){ .error = true };    // invalid
                    }
                }
            }

            type = CJ5_TOKEN_NUMBER;
        }
    }

    token->type = type;
    token->num_type =
        (type == CJ5_TOKEN_STRING) ? cj5__hash_fnv32(&json5[start], &json5[parser->pos]) : num_type;
    token->start = start;
    token->end = parser->pos;
    token->parent_id = parser->super_id;
    --parser->pos;
    return (cj5_result){ 0 };
}

static cj5_result cj5__parse_string(cj5__parser* parser, const char* json5, int len,
                                    cj5_token* tokens, int max_tokens)
{
    cj5_token* token;
    int start = parser->pos;
    char str_open = json5[start];
    ++parser->pos;

    for (; parser->pos < len; parser->pos++) {
        char c = json5[parser->pos];

        // end of string
        if (str_open == c) {
            if (tokens == NULL) {
                return (cj5_result){ 0 };
            }

            token = cj5__alloc_token(parser, tokens, max_tokens);
            token->type = CJ5_TOKEN_STRING;
            token->start = start + 1;
            token->end = parser->pos;
            token->parent_id = parser->super_id;

            return (cj5_result){ 0 };
        }

        if (c == '\\' && parser->pos + 1 < len) {
            ++parser->pos;
            switch (json5[parser->pos]) {
            case '\"':
            case '/':
            case '\\':
            case 'b':
            case 'f':
            case 'r':
            case 'n':
            case 't':
                break;
            case 'u':
                ++parser->pos;
                for (int i = 0; i < 4 && parser->pos < len; i++) {
                    /* If it isn't a hex character we have an error */
                    if (!((json5[parser->pos] >= 48 && json5[parser->pos] <= 57) ||   /* 0-9 */
                          (json5[parser->pos] >= 65 && json5[parser->pos] <= 70) ||   /* A-F */
                          (json5[parser->pos] >= 97 && json5[parser->pos] <= 102))) { /* a-f */
                        parser->pos = start;
                        return (cj5_result){ .error = true };    // invalid
                    }
                    parser->pos++;
                }

                --parser->pos;
                break;
            case '\n':
                break;
            default:
                parser->pos = start;
                return (cj5_result){ .error = true };    // invalid escape
            }
        }
    }

    parser->pos = start;
    return (cj5_result){ .error = true };    // incomplete
}

static void cj5__skip_comment(cj5__parser* parser, const char* json5, int len)
{
    for (; parser->pos < len; parser->pos++) {
        if (json5[parser->pos] == '\n' || json5[parser->pos] == '\r') {
            return;
        }
    }
}

cj5_result cj5_parse(const char* json5, int len, cj5_token* tokens, int max_tokens)
{
    cj5__parser parser = { .super_id = -1 };
    cj5_result r = { 0 };
    cj5_token* token;
    int count = parser.next_id;
    int line = 0;
    bool can_comment = false;

    for (; parser.pos < len; parser.pos++) {
        char c;
        cj5_token_type type;

        c = json5[parser.pos];
        switch (c) {
        case '{':
        case '[':
            can_comment = false;
            count++;
            if (tokens == NULL) {
                break;
            }
            token = cj5__alloc_token(&parser, tokens, max_tokens);
            if (token == NULL) {
                r.error = true;
                return r;    // out of memory
            }

            if (parser.super_id != -1) {
                cj5_token* super_token = &tokens[parser.super_id];
                token->parent_id = parser.super_id;
                ++super_token->size;
            }

            token->type = (c == '{' ? CJ5_TOKEN_OBJECT : CJ5_TOKEN_ARRAY);
            token->start = parser.pos;
            parser.super_id = parser.next_id - 1;
            break;

        case '}':
        case ']':
            can_comment = false;
            if (tokens == NULL) {
                break;
            }
            type = (c == '}' ? CJ5_TOKEN_OBJECT : CJ5_TOKEN_ARRAY);

            if (parser.next_id < 1) {
                r.error = true;
                return r;    // invalid character
            }

            token = &tokens[parser.next_id - 1];
            for (;;) {
                if (token->start != -1 && token->end == -1) {
                    if (token->type != type) {
                        r.error = true;
                        return r;    // invalid
                    }
                    token->end = parser.pos + 1;
                    parser.super_id = token->parent_id;
                    break;
                }

                if (token->parent_id == -1) {
                    if (token->type != type || parser.super_id == -1) {
                        r.error = true;
                        return r;    // invalid
                    }
                    break;
                }

                token = &tokens[token->parent_id];
            }
            break;

        case '\"':
        case '\'':
            can_comment = false;
            // JSON5: strings can start with \" or \'
            r = cj5__parse_string(&parser, json5, len, tokens, max_tokens);
            if (r.error) {
                return r;
            }
            count++;
            if (parser.super_id != -1 && tokens) {
                if (++tokens[parser.super_id].size == 1 &&
                    tokens[parser.super_id].type == CJ5_TOKEN_STRING) {
                    // it's not a value, it's a key, so hash it
                    tokens[parser.super_id].key_hash = cj5__hash_fnv32(
                        &json5[tokens[parser.super_id].start], &json5[tokens[parser.super_id].end]);
                }
            }
            break;

        case '\r':
        case '\n':
            line++;
            can_comment = true;
            break;
        case '\t':
        case ' ':
            break;

        case ':':
            can_comment = false;
            parser.super_id = parser.next_id - 1;
            break;

        case ',':
            can_comment = false;
            if (token != NULL && parser.super_id != -1 &&
                tokens[parser.super_id].type != CJ5_TOKEN_ARRAY &&
                tokens[parser.super_id].type != CJ5_TOKEN_OBJECT) {
                parser.super_id = tokens[parser.super_id].parent_id;
            }
            break;
        case '/':
            if (can_comment && parser.pos < len - 1 && json5[parser.pos + 1] == '/') {
                cj5__skip_comment(&parser, json5, len);
                break;
            }

        default:
            r = cj5__parse_primitive(&parser, json5, len, tokens, max_tokens);
            if (r.error) {
                return r;
            }
            can_comment = false;
            count++;
            if (parser.super_id != -1 && tokens != NULL) {
                if (++tokens[parser.super_id].size == 1 &&
                    tokens[parser.super_id].type == CJ5_TOKEN_STRING) {
                    tokens[parser.super_id].key_hash = cj5__hash_fnv32(
                        &json5[tokens[parser.super_id].start], &json5[tokens[parser.super_id].end]);
                }
            }
            break;
        }
    }

    if (tokens) {
        for (int i = parser.next_id - 1; i >= 0; i--) {
            // unmatched object or array ?
            if (tokens[i].start != -1 && tokens[i].end == -1) {
                r.error = true;
                return r;    // incomplete
            }
        }
    }

    r.num_tokens = count;
    r.tokens = tokens;
    r.json5 = json5;
    return r;
}

#ifdef CJ5_TEST
#    include <stdio.h>
#    include <stdlib.h>
#    include <string.h>

const char* g_token_types[] = { "CJ5_TOKEN_OBJECT", "CJ5_TOKEN_ARRAY", "CJ5_TOKEN_NUMBER",
                                "CJ5_TOKEN_STRING", "CJ5_TOKEN_BOOL",  "CJ5_TOKEN_NULL" };

const char* g_json =
    "{ test: 1, test2: null, \n// this is child\n//another line\n child: {some_string: \"halo "
    "wurst\", array: [1, 2, '3.5'], }, hex:0xcecece, }";
int main()
{
    cj5_token tokens[64];
    cj5_result r = cj5_parse(g_json, (int)strlen(g_json), tokens, 64);

    puts(g_json);

    if (!r.error) {
        for (int i = 0; i < r.num_tokens; i++) {
            char content[1024];
            int num = tokens[i].end - tokens[i].start;
            memcpy(content, &g_json[tokens[i].start], num);
            content[num] = '\0';
            assert(tokens[i].type <= CJ5_TOKEN_NULL);
            printf("%d: { type = %s, size = %d, parent = %d, content = '%s'\n", i,
                   g_token_types[tokens[i].type], tokens[i].size, tokens[i].parent_id, content);
        }
    } else {
        puts("error");
    }
    return 0;
}

int cj5_seek(cj5_result* r, int parent_id, const char* key)
{
    CJ5_ASSERT(parent_id >= 0 && parent_id < r->num_tokens);

    uint32_t key_hash = cj5__hash_fnv32(key, key + cj5__strlen(key));
    const cj5_token* parent_tok = &r->tokens[parent_id];

    for (int i = parent_id + 1, count = 0; i < r->num_tokens && count < parent_tok->size; i++) {
        const cj5_token* tok = &r->tokens[i];

        if (tok->size != 1 || tok->type != CJ5_TOKEN_STRING) {
            continue;
        }

        if (parent_id == tok->parent_id) {
            if (key_hash == tok->key_hash) {
                CJ5_ASSERT((i + 1) < r->num_tokens);
                return i + 1;    // return next "value" token (array/objects and primitive values)
            }
            count++;
        }
    }

    return -1;
}

static int cj5__seek_recursive(cj5_result* r, int parent_id, uint32_t key_hash)
{
    const cj5_token* parent_tok = &r->tokens[parent_id];

    for (int i = parent_id + 1, count = 0; i < r->num_tokens && count < parent_tok->size; i++) {
        const cj5_token* tok = &r->tokens[i];

        if (tok->size != 1 || tok->type != CJ5_TOKEN_STRING) {
            continue;
        }

        if (parent_id == tok->parent_id) {
            CJ5_ASSERT((i + 1) < r->num_tokens);
            if (key_hash == tok->key_hash) {
                return i + 1;    // return next "value" token (array/objects and primitive values)
            } else if (r->tokens[i + 1].size) {
                int found_id = cj5__seek_recursive(r, i + 1, key_hash);
                if (found_id != -1) {
                    return found_id;
                }
            }
            count++;
        }
    }

    return -1;
}

static inline char* cj5__strcpy(char* CJ5__RESTRICT dst, int dst_sz, const char* CJ5__RESTRICT src,
                                int num)
{
    const int _max = dst_sz - 1;
    const int _num = (num < _max ? num : _max);
    if (_num > 0) {
        CJ5_MEMCPY(dst, src, _num);
    }
    dst[_num] = '\0';
    return dst;
}

static bool cj5__tofloat(const char* str, double* ofloat)
{
    // skip leading whitespace
    while (*str && cj5__isspace(*str)) {
        str++;
    }
    int sign = 1;

    const char* pC = str;
    if (*pC == '-') {
        sign = -1;
        pC++;
    }

    // parse integer part
    int64_t tmp = 0;
    while (*pC >= '0' && *pC <= '9') {
        tmp *= 10;
        tmp += *pC - '0';
        pC++;
    }

    if (*pC == 0) {
        *ofloat = (double)(sign >= 0 ? tmp : -tmp);
        return true;
    }

    // Parse fraction
    if (*pC == '.') {
        pC++;

        int64_t divisor = sign;
        while (*pC >= '0' && *pC <= '9') {
            divisor *= 10;
            tmp *= 10;
            tmp += *pC - '0';
            pC++;
        }

        // Skip tailing whitespace
        while (*pC && cj5__isspace(*pC)) {
            pC++;
        }

        if (*pC == 0) {
            *ofloat = (double)tmp / (double)divisor;
            return true;
        }
    }

    return sscanf(str, "%lf", ofloat) == 1;
}

static bool cj5__toint(const char* str, int* oint)
{
    const int sign = (*str == '-') ? -1 : 1;
    str += (1 - sign) / 2;
    if (!*str) {
        return false;
    }
    int result = 0;
    for (;;) {
        char c = *(str++);
        if (cj5__isnum(c)) {
            result = result * 10 + c;
        } else {
            *oint = result * sign;
            return true;
        }
    }
}

static bool cj5__touint(const char* str, int* oint)
{
    if (*str == '-') {
        return false;
    }
    uint32_t result = 0;
    for (;;) {
        char c = *(str++);
        if (cj5__isnum(c)) {
            result = result * 10 + (uint32_t)c;
        } else {
            *oint = result;
            return true;
        }
    }
}

int cj5_seek_recursive(cj5_result* r, int parent_id, const char* key)
{
    CJ5_ASSERT(parent_id >= 0 && parent_id < r->num_tokens);

    uint32_t key_hash = cj5__hash_fnv32(key, key + cj5__strlen(key));
    return cj5__seek_recursive(r, parent_id, key_hash);
}

const char* cj5_get_string(cj5_result* r, int id, char* str, int max_str)
{
    CJ5_ASSERT(id >= 0 && id < r->num_tokens);
    const cj5_token* tok = &r->tokens[id];
    CJ5_ASSERT(tok->type == CJ5_TOKEN_STRING);
    return cj5__strcpy(str, max_str, &r->json5[tok->start], tok->end - tok->start);
}

double cj5_get_double(cj5_result* r, int id)
{
    CJ5_ASSERT(id >= 0 && id < r->num_tokens);
    const cj5_token* tok = &r->tokens[id];
    CJ5_ASSERT(tok->type == CJ5_TOKEN_NUMBER);
    char snum[32];
    double num;
    cj5__strcpy(snum, sizeof(snum), &r->json5[tok->start], tok->end - tok->start);
    bool ok = cj5__tofloat(snum, &num);
    CJ5_ASSERT(ok);
    CJ5__UNUSED(ok);
    return num;
}

int cj5_get_int(cj5_result* r, int id)
{
    CJ5_ASSERT(id >= 0 && id < r->num_tokens);
    const cj5_token* tok = &r->tokens[id];
    CJ5_ASSERT(tok->type == CJ5_TOKEN_NUMBER);
    char snum[32];
    int num;
    cj5__strcpy(snum, sizeof(snum), &r->json5[tok->start], tok->end - tok->start);
    bool ok = cj5__toint(snum, &num);
    CJ5_ASSERT(ok);
    CJ5__UNUSED(ok);
    return num;
}

uint32_t cj5_get_uint(cj5_result* r, int id)
{
    CJ5_ASSERT(id >= 0 && id < r->num_tokens);
    const cj5_token* tok = &r->tokens[id];
    CJ5_ASSERT(tok->type == CJ5_TOKEN_NUMBER);
    char snum[32];
    uint32_t num;
    cj5__strcpy(snum, sizeof(snum), &r->json5[tok->start], tok->end - tok->start);
    bool ok = cj5__touint(snum, &num);
    CJ5_ASSERT(ok);
    CJ5__UNUSED(ok);
    return num;
}

uint64_t cj5_get_uint64(int id);
int64_t cj5_get_int64(int id);
bool cj5_get_bool(int id);

double cj5_seekget_double(int parent_id, const char* key);
float cj5_seekget_float(int parent_id, const char* key);
int cj5_seekget_int(int parent_id, const char* key);
uint32_t cj5_seekget_uint(int parent_id, const char* key);
uint64_t cj5_seekget_uint64(int parent_id, const char* key);
int64_t cj5_seekget_int64(int parent_id, const char* key);
bool cj5_seekget_bool(int parent_id, const char* key);
const char* cj5_seekget_string(int parent_id, const char* key, char* str, int max_str);

int cj5_seekget_array_double(int parent_id, const char* key, double* values, int max_values);
int cj5_seekget_array_float(int parent_id, const char* key, float* values, int max_values);
int cj5_seekget_array_int(int parent_id, const char* key, int* values, int max_values);
int cj5_seekget_array_uint(int parent_id, const char* key, uint32_t* values, int max_values);
int cj5_seekget_array_uint64(int parent_id, const char* key, uint64_t* values, int max_values);
int cj5_seekget_array_int64(int parent_id, const char* key, int64_t* values, int max_values);
int cj5_seekget_array_bool(int parent_id, const char* key, bool* values, int max_values);
int cj5_seekget_array_string(int parent_id, const char* key, char** strs, int max_str);

int cj5_get_array_count(int id);
int cj5_get_array_elem(int id, int index);
int cj5_get_child_count(int id);

#    if 0
void wish()
{
    int cj5_dom_seek(parent_id, key);
    int cj5_dom_seek_recursive(parent_id, key);

    const char* cj5_dom_get_string(id, str, max_str);
    TYPE cj5_dom_get_TYPE(id);
    int cj5_dom_get_array_count(id);
    int cj5_dom_get_array_elem(id, index);
    int cj5_dom_get_child_count(id);

    int cj5_dom_seekget_array_TYPE(parent_id, key, array, max_array);
    int cj5_dom_seekget_array_string(parent_id, key, strings, max_strings, max_string_len);
    int cj5_dom_seekget_string(parent_id, key, str, max_str);
    TYPE cj5_dom_seekget_type(parent_id, key);
}
#    endif
#endif    // CJ5_TEST