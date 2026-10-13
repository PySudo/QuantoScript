[« Previous: Error Handling](12-error-handling.md) · [Guide Index](README.md) · [Next: Standard Library »](14-standard-library.md)

# 13. Imports

Imports let you reuse code from another file. QuantoScript imports are *lazy*:
importing a file brings its names into scope without running its top-level
statements as side effects.

---

## Importing specific names

```qscript
from "tools.qs" import APP_NAME, greet, double

print(APP_NAME)
print(greet("Sara"))
print(double(7))
```

Only the names you list become available.

---

## Importing everything

Use `*` to import every name a module defines:

```qscript
from "tools.qs" import *

print(VERSION)
print(hidden_helper())
```

---

## A module is just a file

Any `.qs` file can be imported. Names at the top level are its exports:

```qscript
# tools.qs
APP_NAME = "QuantoScript"
VERSION = 1

func greet(name:string, ending:string = "!") {
    return "hello " + name + ending
}

func double(number:int) {
    return number * 2
}

func hidden_helper() {
    return "only imported when named"
}

print("this should not print during import")
```

Importing `tools.qs` does not print that last line — normal statements are not
executed on import. Names are read from the file and made available to the
importer.

---

## Where modules are found

Import paths are resolved relative to the importing file, and also against
`QUANTO_HOME` (so `from "stdlib/http.qs" import get` finds a standard-library
module) and the current working directory.

```qscript
from "stdlib/json.qs" import parse, stringify
from "../shared/helpers.qs" import clamp_value
```

Standard-library module paths begin with `stdlib/`; package paths begin with
`packages/`. See [Package Manager](19-package-manager.md).

---

## Standard library vs runtime

The modules under `stdlib/` are ordinary QuantoScript files. They are backed by
hidden native built-ins (names beginning with `sys_`) that user code normally
does not call directly. The next chapter tours the public modules.

---

## Next steps

[Standard Library](14-standard-library.md) covers the modules that ship with
QuantoScript.

---

[« Previous: Error Handling](12-error-handling.md) · [Guide Index](README.md) · [Next: Standard Library »](14-standard-library.md)
