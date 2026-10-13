[« Previous: Operators](04-operators.md) · [Guide Index](README.md) · [Next: Lists »](06-lists.md)

# 5. Strings

Strings hold text. QuantoScript strings are UTF-8 and use **1-based** indexing,
like the rest of the language's collections.

---

## Writing strings

Single and double quotes are equivalent, and either quote may appear inside a
string. A quote only ends the string when it is followed by the end of the line
or by punctuation and operators such as `,`, `)`, `]`, `.`, or `+`:

```qscript
single   = 'hello'
double   = "world"
opposite = "i use ' inside this string."
same     = "i use " inside this string."
```

### Escape sequences

These sequences are decoded when the string is read:

| Sequence | Produces | Example |
|----------|----------|---------|
| `\n` | newline | `"line1\nline2"` |
| `\r` | carriage return | `"a\rb"` |
| `\t` | tab | `"col1\tcol2"` |
| `\\` | backslash | `"path\\to\\file"` |
| `\"` | double quote | `"say \"hi\""` |
| `\'` | single quote | `"it\'s"` |
| `\0` | null byte | `"a\0b"` |

```qscript
print("line1\nline2")
print("tab\there")
print("back\\slash")
```

---

## Joining and repeating

```qscript
message = "hello" + " " + "world"
print(message)          # hello world

print("ha" * 3)         # hahaha
print(3 * "yo")         # yoyoyo
```

---

## Indexing

Indexes start at `1`. Negative indexes count from the end, with `-1` being the
last character:

```qscript
word = "Quanto"
print(word[1])     # Q
print(word[2])     # u
print(word[-1])    # o
```

---

## Slicing

A slice `start:end` returns a substring. Both ends are inclusive, and either
side may be omitted:

```qscript
text = "simple"
print(text[2:5])    # impl
print(text[:3])     # sim
print(text[3:])     # ple
```

Slices on out-of-range indexes are clamped rather than reported as errors.

---

## Length and conversion

```qscript
print(len("Quanto"))    # 6
print(str(42))          # "42"
```

`len` returns the number of characters; `str` converts any value to its string
form.

---

## String methods

| Method | Description | Example | Result |
|--------|-------------|---------|--------|
| `contains(part)` | test for a substring | `"hello world".contains("world")` | `true` |
| `replace(old, new)` | replace every occurrence | `"aabbcc".replace("bb", "xx")` | `aaxxcc` |
| `startsWith(part)` | test the beginning | `"hello".startsWith("he")` | `true` |
| `endsWith(part)` | test the end | `"hello".endsWith("lo")` | `true` |
| `title()` | capitalise each word | `"hello world".title()` | `Hello World` |
| `split(sep)` | break into a list | `"a,b,c".split(",")` | `['a', 'b', 'c']` |
| `join(list)` | join a list into a string | `", ".join(["a", "b"])` | `a, b` |
| `indexOf(part)` | first position of `part`, or `0` | `"Quanto".indexOf("an")` | `3` |
| `substring(start, end)` | text from `start` to `end` (inclusive) | `"Quanto".substring(2, 4)` | `uan` |
| `trimStart()` | strip leading whitespace | `"  pad".trimStart()` | `pad` |
| `trimEnd()` | strip trailing whitespace | `"pad  ".trimEnd()` | `pad` |
| `repeat(n)` | repeat the text `n` times | `"ab".repeat(3)` | `ababab` |
| `reverse()` | reverse the characters | `"abc".reverse()` | `cba` |

```qscript
print("hello world".contains("world"))        # true
print("hello world".replace("world", "QS"))   # hello QS
print("hello".startsWith("he"))               # true
print("hello".endsWith("lo"))                 # true
print("hello world".title())                  # Hello World
print("red,green,blue".split(","))            # ['red', 'green', 'blue']
print("abc".split(""))                        # ['a', 'b', 'c']
print(", ".join(["Ali", "Sara", "Mina"]))     # Ali, Sara, Mina
print("Quanto".indexOf("an"))                 # 3
print("Quanto".substring(2, 4))               # uan
print("  pad  ".trimStart() + "|")            # pad  |
print("  pad  ".trimEnd() + "|")              #   pad|
print("ab".repeat(3))                         # ababab
print("abc".reverse())                        # cba
```

> Uppercase and lowercase conversion live in the standard library rather than as
> methods. Import them from `stdlib/text.qs`:
>
> ```qscript
> from "../stdlib/text.qs" import lower, upper
> print(lower("HELLO"))   # hello
> print(upper("hello"))   # HELLO
> ```

---

## Iterating

You can loop over a string directly to visit one character at a time:

```qscript
repeat letter -> "abc" {
    print(letter)
}
```

See [Control Flow](08-control-flow.md) for the full set of iteration forms.

---

## Next steps

Lists are the subject of [Lists](06-lists.md).

---

[« Previous: Operators](04-operators.md) · [Guide Index](README.md) · [Next: Lists »](06-lists.md)
