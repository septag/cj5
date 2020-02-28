#pragma once

#include <stdbool.h>    // bool
#include <stdint.h>     // uint32_t, int64_t, etc.

#ifndef CJ5_ASSERT
#    include <assert.h>
#    define CJ5_ASSERT(_e) assert(_e)
#endif

typedef enum cj5_token_type {
    CJ5_TOKEN_OBJECT = 0,
    CJ5_TOKEN_ARRAY,
    CJ5_TOKEN_NUMBER,
    CJ5_TOKEN_STRING,
    CJ5_TOKEN_BOOL,
    CJ5_TOKEN_NULL
} cj5_token_type;

typedef struct cj5_result {
    bool error;
    int error_line;
    int error_pos;
    int num_tokens;
} cj5_result;

typedef struct cj5_token {
    cj5_token_type type;
    int start;
    int end;
    int size;
    int parent_id;
} cj5_token;

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

    cj5_token* token = &tokens[parser->next_id];
    token->start = token->end = -1;
    token->size = 0;
    token->parent_id = -1;
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
        return (cj5_result) {0};
    }
    token = cj5__alloc_token(parser, tokens, max_tokens);
    if (token == NULL) {
        parser->pos = start;
        return (cj5_result) {.error = true };   // out of memory
    }

    cj5_token_type type;
    if (keyname) {
        // JSON5: it is likely a key-name, validate and interpret as string
        for (int i = start; i < parser->pos; i++) {
            if (cj5__islowerchar(json5[i]) || cj5__isupperchar(json5[i]) || json5[i] == '_') {
                continue;
            }

            if (cj5__isnum(json5[i])) {
                if (i == start) {
                    parser->pos = start;
                    return (cj5_result) {.error = true};    // invalid
                }
                continue;
            }

            parser->pos = start;
            return (cj5_result) {.error = true};    // invalid
        }

        type = CJ5_TOKEN_STRING;
    }

    *token = (cj5_token) {
        .type = type,
        .start = start,
        .end = parser->pos,
        .parent_id = parser->super_id
    };
    --parser->pos;
    return (cj5_result) {0};
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

cj5_result cj5_parse(const char* json5, uint32_t len, cj5_token* tokens, int max_tokens)
{
    cj5__parser parser = { .super_id = -1 };
    cj5_result r = { 0 };
    cj5_token* token;
    int count = parser.next_id;

    for (; parser.pos < len; parser.pos++) {
        char c;
        cj5_token_type type;

        c = json5[parser.pos];
        switch (c) {
        case '{':
        case '[':
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
            // JSON5: strings can start with \" or \'
            r = cj5__parse_string(&parser, json5, len, tokens, max_tokens);
            if (r.error) {
                return r;
            }
            count++;
            if (parser.super_id != -1 && tokens) {
                ++tokens[parser.super_id].size;
            }
            break;

        case '\t':
        case '\r':
        case '\n':
        case ' ':
            break;

        case ':':
            parser.super_id = parser.next_id - 1;
            break;

        case ',':
            if (token != NULL && parser.super_id != -1 &&
                tokens[parser.super_id].type != CJ5_TOKEN_ARRAY &&
                tokens[parser.super_id].type != CJ5_TOKEN_OBJECT) {
                parser.super_id = tokens[parser.super_id].parent_id;
            }
            break;

        default:
            r = cj5__parse_primitive(&parser, json5, len, tokens, max_tokens);
            if (r.error) {
                return r;
            }
            count++;
            if (parser.super_id != -1 && tokens != NULL) {
                ++tokens[parser.super_id].size;
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
    return r;
}
