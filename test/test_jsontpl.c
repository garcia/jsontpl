#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "verify.h"
#include "jsontpl.h"

typedef struct {
    char *name;
    char *json;
    char *tpl;
    const char **expected;
} test_case;

test_case tests[] = {
    
    /* Literals */
    
       {"literal output",
        "{}",
        "ABCEFGHIJKLMNOPQRSTUVWXYZ abcdefghijklmnopqrstuvwxyz\t0123456789\n`~!@#$%^&*()-_=+|[{]};:'\",<.>/?",
        (const char *[]){"ABCEFGHIJKLMNOPQRSTUVWXYZ abcdefghijklmnopqrstuvwxyz\t0123456789\n`~!@#$%^&*()-_=+|[{]};:'\",<.>/?", NULL}
    }, {"literal output with backslashes",
        "{}",
        "\\A\\Z\\a\\z\\0\\9\\~\\",
        (const char *[]){"\\A\\Z\\a\\z\\0\\9\\~\\", NULL}
    }, {"backslash escape sequences",
        "{}",
        "\\\\ \\{% %\\} \\{= =\\} \\{\\}",
        (const char *[]){"\\ {% %} {= =} {}", NULL}
    
    /* Values */
    
    }, {"basic values",
        "{\"alpha\": null, \"beta\": false, \"gamma\": true, \"delta\": 42, \"epsilon\": 3.125, \"zeta\": \"foobar\"}",
        "{= alpha =} {= beta =} {= gamma =} {= delta =} {= epsilon =} {= zeta =}",
        (const char *[]){"null false true 42 3.125 foobar", NULL}
    }, {"value spacing",
        "{\"alpha\": null, \"beta\": false, \"gamma\": true, \"delta\": 42, \"epsilon\": 3.125, \"zeta\": \"foobar\"}",
        "{= alpha=} {=beta=}{=gamma=} {=delta =} {= epsilon =} {=\tzeta\t=}",
        (const char *[]){"null falsetrue 42 3.125 foobar", NULL}
    }, {"variable names",
        "{\"alpha\": null, \"beta\": false, \"gamma\": true, \"delta\": 42, \"names\": [\"alpha\", \"beta\", \"gamma\", \"delta\"]}",
        "{% foreach names: name %}{= {name} =} {% end %}",
        (const char *[]){"null false true 42 ", NULL}
    }, {"variable object keys",
        "{\"obj\": {\"alpha\": null, \"beta\": false, \"gamma\": true, \"delta\": 42}, \"names\": [\"alpha\", \"beta\", \"gamma\", \"delta\"]}",
        "{% foreach names: name %}{= obj.{name} =} {% end %}",
        (const char *[]){"null false true 42 ", NULL}
    
    /* Blocks */
    
    }, {"foreach array",
        "{\"array\": [123, 456, 789]}",
        "Array contents: [ {% foreach array: a %}{= a =} {% end %}]",
        (const char *[]){"Array contents: [ 123 456 789 ]", NULL}
    }, {"foreach empty array",
        "{\"array\": []}",
        "Array contents: [ {% foreach array: a %}{= a =} {% end %}]",
        (const char *[]){"Array contents: [ ]", NULL}
    }, {"foreach array with shadowed value",
        "{\"array\": [123, 456, 789], \"a\": \"|\"}",
        "Array contents: {= a =} {% foreach array: a %}{= a =} {% end %}{= a =}",
        (const char *[]){"Array contents: | 123 456 789 |", NULL}
    }, {"foreach object",
        "{\"object\": {\"alpha\": null, \"delta\": 42, \"zeta\": \"foobar\"}}",
        "Object contents: \\{ {% foreach object: k -> v %}{= k =}->{= v =} {% end %}\\}",
        (const char *[]){
            "Object contents: { alpha->null delta->42 zeta->foobar }",
            "Object contents: { alpha->null zeta->foobar delta->42 }",
            "Object contents: { delta->42 alpha->null zeta->foobar }",
            "Object contents: { zeta->foobar alpha->null delta->42 }",
            "Object contents: { delta->42 zeta->foobar alpha->null }",
            "Object contents: { zeta->foobar delta->42 alpha->null }",
            NULL}
    }, {"foreach empty object",
        "{\"object\": {}}",
        "Object contents: \\{ {% foreach object: k -> v %}{= k =}->{= v =} {% end %}\\}",
        (const char *[]){"Object contents: { }", NULL}
    }, {"foreach object with shadowed value",
        "{\"object\": {\"alpha\": null, \"delta\": 42, \"zeta\": \"foobar\"}, \"k\": \"<\", \"v\": \">\"}",
        "Object contents: {= k =}{= v =} {% foreach object: k -> v %}{= k =}->{= v =} {% end %}{= k =}{= v =}",
        (const char *[]){
            "Object contents: <> alpha->null delta->42 zeta->foobar <>",
            "Object contents: <> alpha->null zeta->foobar delta->42 <>",
            "Object contents: <> delta->42 alpha->null zeta->foobar <>",
            "Object contents: <> zeta->foobar alpha->null delta->42 <>",
            "Object contents: <> delta->42 zeta->foobar alpha->null <>",
            "Object contents: <> zeta->foobar delta->42 alpha->null <>",
            NULL}
    }, {"if false",
        "{\"one\": null, \"two\": false, \"three\": 0, \"four\": 0.0, \"five\": \"\", \"six\": [], \"seven\": {}}",
        "{% if zero %}{= zero =}{% end %}{% if one %}{= one =}{% end %}{% if two %}{= two =}{% end %}{% if three %}{= three =}{% end %}{% if four %}{= four =}{% end %}{% if five %}{= five =}{% end %}{% if six %}{= six =}{% end %}{% if seven %}{= seven =}{% end %}",
        (const char *[]){"", NULL}
    }, {"if true",
        "{\"one\": true, \"two\": -1, \"three\": 100.5, \"four\": \"string\", \"five\": [1, 2, 3], \"six\": {\"key\": \"value\"}}",
        "{% if one %}{= one =}{% end %}{% if two %}{= two =}{% end %}{% if three %}{= three =}{% end %}{% if four %}{= four =}{% end %}{% if five %}(array){% end %}{% if six %}(object){% end %}",
        (const char *[]){"true-1100.5string(array)(object)", NULL}
    }, {"if array value",
        "{\"array\": [-2, -1, 0, 1, 2]}",
        "{% foreach array: a %}{% if a %}{= a =} {% end %}{% end %}",
        (const char *[]){"-2 -1 1 2 ", NULL}
    }, {"if object key exists",
        "{\"object\": {\"one\": {\"required_value\": true}, \"two\": {\"required_value\": true, \"optional_value\": true}, \"three\": {\"required_value\": true}}}",
        "{% foreach object: k -> v %}{= k =}: {= v.required_value =}{% if v.optional_value %} ({= v.optional_value =}){% end %}; {% end %}",
        (const char *[]){"one: true; two: true (true); three: true; ", NULL}
    }, {"if-else",
        "{\"alpha\": true}",
        "({% if alpha %}Alpha{% else %}Beta{% end %} {% if beta %}Beta{% else %}Alpha{% end %})",
        (const char *[]){"(Alpha Alpha)", NULL}
    }, {"nested if-else",
        "{\"alpha\": true}",
        "({% if alpha %}Alpha{% if beta %}Beta{% else %}Alpha{% end %}{% end %} {% if beta %}Beta{% if alpha %}Alpha{% else %}Beta{% end %}{% end %})",
        (const char *[]){"(AlphaAlpha )", NULL}
    
    /* Filters */
    
    }, {"upper filter",
        "{\"string\": \"This is a JSON string.\"}",
        "{= string | upper =}",
        (const char *[]){"THIS IS A JSON STRING.", NULL}
    }, {"lower filter",
        "{\"string\": \"This is a JSON string.\"}",
        "{= string | lower =}",
        (const char *[]){"this is a json string.", NULL}
    }, {"identifier filter",
        "{\"string\": \"This is a string.\"}",
        "{= string | identifier =}",
        (const char *[]){"This_is_a_string_", NULL}
    }, {"count filter",
        "{\"array\": [1, 2, 3], \"object\": {\"foo\": \"bar\", \"baz\": \"quux\"}}",
        "{= array | count =} {= object | count =}",
        (const char *[]){"3 2", NULL}
    }, {"english filter",
        "{\"empty\": [], \"one\": [\"alpha\"], \"two\": [\"alpha\", \"beta\"], \"three\": [\"alpha\", \"beta\", \"gamma\"], \"four\": [\"alpha\", \"beta\", \"gamma\", \"delta\"]}",
        "{= empty | english =}; {= one | english =}; {= two | english =}; {= three | english =}; {= four | english =}",
        (const char *[]){"; alpha; alpha and beta; alpha, beta, and gamma; alpha, beta, gamma, and delta", NULL}
    }, {"js filter",
        // TODO: array / object tests
        "{\"alpha\": null, \"beta\": false, \"gamma\": true, \"delta\": 42, \"epsilon\": 3.125, \"zeta\": \"foobar\"}",
        "{= alpha | js =} {= beta | js =} {= gamma | js =} {= delta | js =} {= epsilon | js =} {= zeta | js =}",
        (const char *[]){"null false true 42 3.125 \"foobar\"", NULL}
    }, {"c filter",
        "{\"alpha\": null, \"beta\": false, \"gamma\": true, \"delta\": 42, \"epsilon\": 3.125, \"zeta\": \"foobar\"}",
        "{= alpha | c =} {= beta | c =} {= gamma | c =} {= delta | c =} {= epsilon | c =} {= zeta | c =}",
        (const char *[]){"NULL 0 1 42 3.125 \"foobar\"", NULL}
    }, {"py filter",
        "{\"alpha\": null, \"beta\": false, \"gamma\": true, \"delta\": 42, \"epsilon\": 3.125, \"zeta\": \"foobar\"}",
        "{= alpha | py =} {= beta | py =} {= gamma | py =} {= delta | py =} {= epsilon | py =} {= zeta | py =}",
        (const char *[]){"None False True 42 3.125 \"foobar\"", NULL}
    
    /* Sentinel value - keep this last */
    }, {NULL}
};

#undef verify_cleanup
#define verify_cleanup free(output)
int run_test(const test_case *test)
{
    const char **expected = NULL;
    char *output = NULL;
    
    verify_call_hint(jsontpl_string(test->json, test->tpl, &output),
        "while testing \"%s\"", test->name);
    
    for (expected = &test->expected[0]; *expected != NULL; expected++) {
        if (strcmp(*expected, output) == 0) {
            verify_return();
        }
    }
    
    verify_fail("\"%s\" test failed\n\nexpected: \t%s\nactual: \t%s",
        test->name, test->expected[0], output);
}

#undef verify_cleanup
#define verify_cleanup
int main(int argc, char *argv[])
{
    const test_case *test;
    
    for (test = &tests[0]; test->name; test++) {
        verify_call(run_test(test));
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