#include <stdio.h>

#define JSMN_PARENT_LINKS
#include "jsmn.h"

int main()
{
    const char* json = "{ \"test\": { \"some_string\": \"hello\" }, \"num\": 1234}";
    jsmntok_t tokens[64];
    jsmn_parser p;
    jsmn_init(&p);
    int r = jsmn_parse(&p, json, strlen(json), tokens, 64);

    if (r > 0) {
        for (int i = 0; i < r; i++) {
            char content[64];
            int num = tokens[i].end - tokens[i].start;
            memcpy(content, &json[tokens[i].start], num);
            content[num] = '\0';
            printf("{ type = %d, size = %d, parent = %d, content = '%s'\n", tokens[i].type, tokens[i].size, tokens[i].parent, content);
        }
    } else {
        puts("error");
    }
    return 0;
}