Here are the numbers in the list: {% foreach numbers: n %}{n} {% end %}
Here are the strings in the list: {% foreach strings: s %}{s} {% end %}
Here are the literals in the list: {% foreach literals: l %}{l} {% end %}
Here are the heterogeneous elements: {% foreach heterogeneous: e %}{e} {% end %}

Here's what the nested list contains:
{% foreach nested: n %}    {% foreach n: e %}{e} {% end %}
{% end %}