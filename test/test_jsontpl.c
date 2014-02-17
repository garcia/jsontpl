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
    
    /* Literals */
    
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
    
    /* Values */
    
    }, {"basic values",
        "{\"alpha\": null, \"beta\": false, \"gamma\": true, \"delta\": 42, \"epsilon\": 3.125, \"zeta\": \"foobar\"}",
        "{= alpha =} {= beta =} {= gamma =} {= delta =} {= epsilon =} {= zeta =}",
        "null false true 42 3.125 foobar"
    }, {"value spacing",
        "{\"alpha\": null, \"beta\": false, \"gamma\": true, \"delta\": 42, \"epsilon\": 3.125, \"zeta\": \"foobar\"}",
        "{= alpha=} {=beta=}{=gamma=} {=delta =} {= epsilon =} {=\tzeta\t=}",
        "null falsetrue 42 3.125 foobar"
    }, {"variable names",
        "{\"alpha\": null, \"beta\": false, \"gamma\": true, \"delta\": 42, \"names\": [\"alpha\", \"beta\", \"gamma\", \"delta\"]}",
        "{% foreach names: name %}{= {name} =} {% end %}",
        "null false true 42 "
    }, {"variable object keys",
        "{\"obj\": {\"alpha\": null, \"beta\": false, \"gamma\": true, \"delta\": 42}, \"names\": [\"alpha\", \"beta\", \"gamma\", \"delta\"]}",
        "{% foreach names: name %}{= obj.{name} =} {% end %}",
        "null false true 42 "
    
    /* Blocks */
    
    }, {"foreach array",
        "{\"array\": [123, 456, 789]}",
        "Array contents: [ {% foreach array: a %}{= a =} {% end %}]",
        "Array contents: [ 123 456 789 ]"
    }, {"foreach empty array",
        "{\"array\": []}",
        "Array contents: [ {% foreach array: a %}{= a =} {% end %}]",
        "Array contents: [ ]"
    }, {"foreach array with shadowed value",
        "{\"array\": [123, 456, 789], \"a\": \"|\"}",
        "Array contents: {= a =} {% foreach array: a %}{= a =} {% end %}{= a =}",
        "Array contents: | 123 456 789 |"
    }, {"foreach object",
        "{\"object\": {\"alpha\": null, \"delta\": 42, \"zeta\": \"foobar\"}}",
        "Object contents: \\{ {% foreach object: k -> v %}{= k =}->{= v =} {% end %}\\}",
        "Object contents: { alpha->null delta->42 zeta->foobar }"
    }, {"foreach empty object",
        "{\"object\": {}}",
        "Object contents: \\{ {% foreach object: k -> v %}{= k =}->{= v =} {% end %}\\}",
        "Object contents: { }"
    }, {"foreach object with shadowed value",
        "{\"object\": {\"alpha\": null, \"delta\": 42, \"zeta\": \"foobar\"}, \"k\": \"<\", \"v\": \">\"}",
        "Object contents: {= k =}{= v =} {% foreach object: k -> v %}{= k =}->{= v =} {% end %}{= k =}{= v =}",
        "Object contents: <> alpha->null delta->42 zeta->foobar <>"
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
    }, {"if-else",
        "{\"alpha\": true}",
        "({% if alpha %}Alpha{% else %}Beta{% end %} {% if beta %}Beta{% else %}Alpha{% end %})",
        "(Alpha Alpha)"
    }, {"nested if-else",
        "{\"alpha\": true}",
        "({% if alpha %}Alpha{% if beta %}Beta{% else %}Alpha{% end %}{% end %} {% if beta %}Beta{% if alpha %}Alpha{% else %}Beta{% end %}{% end %})",
        "(AlphaAlpha )"
    
    /* Filters */
    
    }, {"upper filter",
        "{\"string\": \"This is a JSON string.\"}",
        "{= string | upper =}",
        "THIS IS A JSON STRING."
    }, {"lower filter",
        "{\"string\": \"This is a JSON string.\"}",
        "{= string | lower =}",
        "this is a json string."
    }, {"identifier filter",
        "{\"string\": \"This is a string.\"}",
        "{= string | identifier =}",
        "This_is_a_string_"
    }, {"count filter",
        "{\"array\": [1, 2, 3], \"object\": {\"foo\": \"bar\", \"baz\": \"quux\"}}",
        "{= array | count =} {= object | count =}",
        "3 2"
    }, {"english filter",
        "{\"empty\": [], \"one\": [\"alpha\"], \"two\": [\"alpha\", \"beta\"], \"three\": [\"alpha\", \"beta\", \"gamma\"], \"four\": [\"alpha\", \"beta\", \"gamma\", \"delta\"]}",
        "{= empty | english =}; {= one | english =}; {= two | english =}; {= three | english =}; {= four | english =}",
        "; alpha; alpha and beta; alpha, beta, and gamma; alpha, beta, gamma, and delta"
    }, {"js filter",
        // TODO: array / object tests
        "{\"alpha\": null, \"beta\": false, \"gamma\": true, \"delta\": 42, \"epsilon\": 3.125, \"zeta\": \"foobar\"}",
        "{= alpha | js =} {= beta | js =} {= gamma | js =} {= delta | js =} {= epsilon | js =} {= zeta | js =}",
        "null false true 42 3.125 \"foobar\""
    }, {"c filter",
        "{\"alpha\": null, \"beta\": false, \"gamma\": true, \"delta\": 42, \"epsilon\": 3.125, \"zeta\": \"foobar\"}",
        "{= alpha | c =} {= beta | c =} {= gamma | c =} {= delta | c =} {= epsilon | c =} {= zeta | c =}",
        "NULL 0 1 42 3.125 \"foobar\""
    }, {"py filter",
        "{\"alpha\": null, \"beta\": false, \"gamma\": true, \"delta\": 42, \"epsilon\": 3.125, \"zeta\": \"foobar\"}",
        "{= alpha | py =} {= beta | py =} {= gamma | py =} {= delta | py =} {= epsilon | py =} {= zeta | py =}",
        "None False True 42 3.125 \"foobar\""
    
    /* Sentinel value - keep this last */
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