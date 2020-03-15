#include <stdio.h>

#define CJ5_IMPLEMENT
#include "../cj5.h"

const char* g_token_types[] = { "CJ5_TOKEN_OBJECT", "CJ5_TOKEN_ARRAY", "CJ5_TOKEN_NUMBER",
                                "CJ5_TOKEN_STRING", "CJ5_TOKEN_BOOL",  "CJ5_TOKEN_NULL" };

const char* g_json =
    "{ test: 1, test2: null,\n \n// this is child\n//another line\n child: {some_string: \"halo "
    "wurst\", array: [1, 2, 3.5], }, hex:0xcecece, }";
int main()
{
    cj5_token tokens[32];
    cj5_result r = cj5_parse(g_json, (int)strlen(g_json), tokens, 32);

    puts(g_json);

    if (!r.error) {
        // enumerate all tokens
        for (int i = 0; i < r.num_tokens; i++) {
            char content[1024];
            int num = tokens[i].end - tokens[i].start;
            memcpy(content, &g_json[tokens[i].start], num);
            content[num] = '\0';
            assert(tokens[i].type <= CJ5_TOKEN_NULL);
            printf("%d: { type = %s, size = %d, parent = %d, content = '%s'\n", i,
                   g_token_types[tokens[i].type], tokens[i].size, tokens[i].parent_id, content);
        }

        // token helper API
        int c = cj5_seek(&r, 0, "child");
        if (c != -1) {
            float v[3];
            cj5_seekget_array_float(&r, c, "array", v, 3);
            char str[32];
            cj5_seekget_string(&r, c, "some_string", str, sizeof(str), "");
            uint32_t h = cj5_seekget_uint(&r, 0, "hex", 0);
            return 0;
        }

    } else {
        printf("ERROR: %d - line:%d (%d)\n", r.error, r.error_line, r.error_col);
        if (r.error == CJ5_ERROR_OVERFLOW) {
            printf("COUNT: %d\n", r.num_tokens);    // actual number of tokens needed
        }
    }
    return 0;
}
