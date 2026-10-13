[« Previous: Getting Started](01-getting-started.md) · [Guide Index](README.md) · [Next: Variables and Types »](03-variables-and-types.md)

# 2. Syntax Basics

QuantoScript is a small, brace-delimited language. This chapter covers the shape
of a source file: how it is commented, split into statements, and grouped into
blocks.

---

## Comments

A `#` begins a comment that runs to the end of the line:

```qscript
# This is a comment
x = 42   # a trailing comment is fine too
```

Block comments use `/* ... */` and may span lines or sit inside an expression:

```qscript
/*
This whole passage is ignored.
*/

answer = 1 /* ignored */ + 2
print(answer)   # 3
```

A line of free text that does not contain an `=` is also treated like a comment,
which is handy for quick notes:

```qscript
This line is a comment too.
```

---

## Statements

Statements are separated by newlines. A typical line is an assignment, a call,
or a control-flow header:

```qscript
name = "Quanto"
print(name)
```

QuantoScript is not sensitive to indentation. Whitespace at the start of a line
is ignored.

---

## Blocks

Groups of statements are wrapped in braces `{ }`. A header line (such as `func`,
`if`, `while`, or `repeat`) introduces a block:

```qscript
func greet(name) {
    print("hello", name)
}

if score > 90 {
    print("A")
}
```

A block may be written on a single line when it contains one statement:

```qscript
func double(x) { return x * 2 }
```

When you use multi-statement blocks, put **one statement per line**. This matters
because the file loader joins physical lines into logical statements until every
`{`, `(`, and `[` is balanced — so single-line formatting inside large blocks is
folded together before parsing.

---

## Identifiers

A name starts with a letter or an underscore and may continue with letters,
digits, and underscores:

```qscript
count = 1
_total = 100
userName = "Sara"
```

---

## Definitions

If an identifier is written entirely in uppercase, QuantoScript treats it as a
*definition* — a value meant to stay constant. Assigning to it again is an error:

```qscript
VERSION = "1.0"
APP_NAME = "QuantoScript"

# VERSION = "2.0"   # error: VERSION already exists
```

Lowercase names are ordinary variables and can be reassigned freely:

```qscript
counter = 0
counter = 10   # fine
```

---

## String quotes

Strings may use single or double quotes, and either quote may appear inside a
string. A quote only ends the string when it is followed by the end of the line
or by punctuation:

```qscript
single   = 'hello'
double   = "world"
opposite = "i use ' inside this string."
same     = "i use " inside this string."
```

See [Strings](05-strings.md) for escape sequences, indexing, and methods.

---

## Putting it together

```qscript
# A tiny program
APP = "QuantoScript"

func banner(text) {
    return "== " + text + " =="
}

print(banner(APP))

/* notes can be long
   and span several lines */
print("done")
```

---

## Next steps

Values and types are the subject of
[Variables and Types](03-variables-and-types.md).

---

[« Previous: Getting Started](01-getting-started.md) · [Guide Index](README.md) · [Next: Variables and Types »](03-variables-and-types.md)
