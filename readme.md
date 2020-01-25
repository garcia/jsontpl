jsontpl
=======

jsontpl is a JSON-oriented templating system written in C.

Introductory example
--------------------

`input.json`:

    {
        "foo": "bar",
        "grocery_list": ["milk", "eggs", "cheese"],
        "author": {
            "name": "Joe Blow",
            "email": "jblow@example.com",
            "website": "https://example.com/"
        }
    }

`input.tpl`:

    The value of foo is {= foo =}.
    
    The grocery list has {= grocery_list|count =} items:
        {% foreach grocery_list: item %}{= item =} {% end %}
    
    jsontpl was written by {= author.name =}.
    Contact me at {% if author.website %}{= author.website =}{% else %}{= author.email =}{% end %} for more information.

`output.txt`:

    The value of foo is bar.
    
    The grocery list has 3 items:
        milk eggs cheese 
    
    jsontpl was written by Ash Garcia.
    Contact me at http://garcia.sh/ for more information.

Values
------

    {= name =}

Values from the JSON input can be substituted into the document by enclosing
a [name](#names) in `{=` and `=}` tokens.

`if` blocks
-----------

    {% if name %} ... {% end %}
    {% if name %} ... {% else %} ... {% end %}

`if` blocks render their contents only if the given value is "truthy".
Truthiness is defined as being `true`, a nonzero number, or a non-empty string
or array or object.

`if` blocks can also check whether the given name exists in the first place.
If the name doesn't point to a value in the JSON object, it's considered
untrue.  But be aware that descending into a nonexistent object is an error:
in the above example, `author.phone_number` is untrue because it doesn't exist,
but `contact.phone_numbers.work_number` is an error because there is no
`phone_numbers` object.

An `if` block may contain an `else` "block" that renders its contents only if
the value *isn't* truthy.  "Block" is in quotes because, unlike other blocks,
it doesn't require its own `{% end %}` marker -- it simply uses the `if`
block's end marker.

`foreach` blocks
----------------

    {% foreach array_name: value %} ... {% end %}
    {% foreach object_name: key -> value %} ... {% end %}

Arrays and objects can be iterated over using a `foreach` block.  Inside the
block, the current value (for arrays) or key and value (for objects) can be
accessed through value substitution.

`comment` blocks
----------------

    {% comment %} ... {% end %}

`comment` blocks are not rendered in the output file.  They behave like an
`if` block that is always untrue (and cannot have an `else` block).

Names
-----

    name
    object_name.name
    name | filter
    {string_name}
    object_name.{string_name}.name

Names are used in the constructs above to retrieve values from the JSON input.
Like JavaScript, objects can be descended into using a `.`, as in `author.name`
in the example at the top of the page.  Each component of a value name can only
contain alphanumeric characters and underscores.

**Filters** are specified by adding a `|` and a filter name.  Filters are
unary: they take one value and produce another value.  These are the filters
currently available:

* `upper`: transform a string to uppercase
* `lower`: transform a string to lowercase
* `identifier`: transform a string to a valid identifier (replaces invalid
  characters with underscores)
* `count`: get the number of items in an array or object
* `english`: turn an array `[A, B, C]` into a string of the form `A, B, and C`
* `js`: produce a valid JavaScript literal (this simply JSON-encodes the value)
* `c`: produce a valid C literal from anything but an array or object
* `py`: produce a valid Python literal from anything but an array or object

**Variable names** are denoted by enclosing the name of a string value in curly
braces.  For example, given a JSON object `{"foo": 1, "bar": "foo"}`, the names
`foo` and `{bar}` are equivalent.  Variable names can be used anywhere in a
name (except as a filter) and can operate recursively, but at the cost of
readability.  Use them sparingly.

Grammar reference
-----------------

    identifier  ::= ("A"..."Z" | "a"..."z" | "0"..."9" | "_")+
    name_component
                ::= identifier | "{" name "}"
    raw_name    ::= name_component | raw_name "." name_component
    filter      ::= "upper" | "lower" | "identifier" | "count" | "english"
                  | "js" | "c" | "py"
    name        ::= raw_name | raw_name "|" filter
    value       ::= "{=" name "=}"
    block_start ::= "{%"
    block_end   ::= "%}"
    if          ::= block_start "if" name block_end
    else        ::= block_start "else" block_end
    foreach     ::= block_start "foreach" name ":" identifier block_end
                  | block_start "foreach" name ":" identifier "->" identifier block_end
    comment     ::= block_start "comment" block_end
    end         ::= block_start "end" block_end
    if_block    ::= if template end | if template else template end
    foreach_block
                ::= foreach template end
    comment_block
                ::= comment template end
    block       ::= if_block | foreach_block | comment_block
    escape      ::= "\{" | "\}" | "\\"
    literal     ::= escape | <any character>
    template    ::= (value | block | literal)*

Literal curly braces do not need to be escaped unless they would otherwise be
interpreted as the start of a block or value.  Likewise, literal backslashes do
not need to be escaped unless they would otherwise be interpreted as an escape
sequence.  To produce the literal sequence of characters `\{=`, one would write
`\\\{=`.

Whitespace (spaces and tabs) may be added between any pair of tokens.  Of
course, whitespace occurring around a `template` token is included in the
template.
