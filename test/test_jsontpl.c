#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "verify.h"
#include "jsontpl.h"

typedef struct {
    char *name;
    char *json;
    char *tpl;
    char *expected;
} test_case;

test_case tests[] = {
       {"literal output",
        "{}",
        "ABCEFGHIJKLMNOPQRSTUVWXYZ abcdefghijklmnopqrstuvwxyz\t0123456789\n`~!@#$%^&*()-_=+|[{]};:'\",<.>/?",
        "ABCEFGHIJKLMNOPQRSTUVWXYZ abcdefghijklmnopqrstuvwxyz\t0123456789\n`~!@#$%^&*()-_=+|[{]};:'\",<.>/?"
    }, {"literal output with backslashes",
        "{}",
        "\\A\\Z\\a\\z\\0\\9\\~\\",
        "\\A\\Z\\a\\z\\0\\9\\~\\"
    }, {"backslash escape sequences",
        "{}",
        "\\\\ \\{% %\\} \\{= =\\} \\{\\}",
        "\\ {% %} {= =} {}"
    }, {"basic values",
        "{\"alpha\": null, \"beta\": false, \"gamma\": true, \"delta\": 42, \"epsilon\": 3.125, \"zeta\": \"foobar\"}",
        "{= alpha =} {= beta =} {= gamma =} {= delta =} {= epsilon =} {= zeta =}",
        "null false true 42 3.125 foobar"
    }, {"value spacing",
        "{\"alpha\": null, \"beta\": false, \"gamma\": true, \"delta\": 42, \"epsilon\": 3.125, \"zeta\": \"foobar\"}",
        "{= alpha=} {=beta=}{=gamma=} {=delta =} {= epsilon =} {=\tzeta\t=}",
        "null falsetrue 42 3.125 foobar"
    }, {"foreach array",
        "{\"array\": [123, 456, 789]}",
        "Array contents: [ {% foreach array: a %}{= a =} {% end %}]",
        "Array contents: [ 123 456 789 ]"
    }, {"foreach empty array",
        "{\"array\": []}",
        "Array contents: [ {% foreach array: a %}{= a =} {% end %}]",
        "Array contents: [ ]"
    }, {"foreach object",
        "{\"object\": {\"alpha\": null, \"delta\": 42, \"zeta\": \"foobar\"}}",
        "Object contents: \\{ {% foreach object: k -> v %}{= k =}->{= v =} {% end %}\\}",
        "Object contents: { alpha->null delta->42 zeta->foobar }"
    }, {"foreach empty object",
        "{\"object\": {}}",
        "Object contents: \\{ {% foreach object: k -> v %}{= k =}->{= v =} {% end %}\\}",
        "Object contents: { }"
    }, {"if false",
        "{\"one\": null, \"two\": false, \"three\": 0, \"four\": 0.0, \"five\": \"\", \"six\": [], \"seven\": {}}",
        "{% if zero %}{= zero =}{% end %}{% if one %}{= one =}{% end %}{% if two %}{= two =}{% end %}{% if three %}{= three =}{% end %}{% if four %}{= four =}{% end %}{% if five %}{= five =}{% end %}{% if six %}{= six =}{% end %}{% if seven %}{= seven =}{% end %}",
        ""
    }, {"if true",
        "{\"one\": true, \"two\": -1, \"three\": 100.5, \"four\": \"string\", \"five\": [1, 2, 3], \"six\": {\"key\": \"value\"}}",
        "{% if one %}{= one =}{% end %}{% if two %}{= two =}{% end %}{% if three %}{= three =}{% end %}{% if four %}{= four =}{% end %}{% if five %}(array){% end %}{% if six %}(object){% end %}",
        "true-1100.5string(array)(object)"
    }, {"if array value",
        "{\"array\": [-2, -1, 0, 1, 2]}",
        "{% foreach array: a %}{% if a %}{= a =} {% end %}{% end %}",
        "-2 -1 1 2 "
    }, {"if object key exists",
        "{\"object\": {\"one\": {\"required_value\": true}, \"two\": {\"required_value\": true, \"optional_value\": true}, \"three\": {\"required_value\": true}}}",
        "{% foreach object: k -> v %}{= k =}: {= v.required_value =}{% if v.optional_value %} ({= v.optional_value =}){% end %}; {% end %}",
        "one: true; two: true (true); three: true; "
    }, {NULL}
};

#undef verify_cleanup
#define verify_cleanup free(output)
int main(int argc, char *argv[])
{
    test_case *test;
    char *output = NULL;
    
    for (test = &tests[0]; test->name; test++) {
        verify_call_hint(jsontpl_string(test->json, test->tpl, &output),
            "while testing \"%s\"", test->name);
        verify(strcmp(test->expected, output) == 0,
            "\"%s\" test failed\n\nexpected: \t%s\nactual: \t%s",
            test->name, test->expected, output);
        free(output);
        output = NULL;
    }
    
    verify_log_("All tests passed");
    verify_return();
}

// literal output
//      escape sequences
// any substitution:
//      at start of file
//      at end of file
//      at start of block
//      at end of block
//      right after another substitution
// value:
//      null, false, true
//      int
//      float
//      string
// any block:
//      inside another block
// if-block:
//      nonexistent value
//      null, false, 0, 0.0, "", [], {}
//      true, any other value
//      false blocks containing nonexistent values
//      false blocks containing multiple nested blocks
// foreach-block:
//      array / object
//      empty array / object
//