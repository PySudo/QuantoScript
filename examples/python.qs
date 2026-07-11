# Python interop demo
# Note: Requires building with Python support: .\build.ps1 -WithPython

try {
    python_init()
    print("Python initialized")

    # Evaluate Python code
    python_eval("import math")
    result = python_eval("math.sqrt(144)")
    print("sqrt(144):", result)

    # Get Python attribute
    pi = python_get("math", "pi")
    print("pi:", pi)

    # Call Python function
    result = python_eval("abs(-42)")
    print("abs(-42):", result)

    # Python string operations
    result = python_eval("'hello'.upper()")
    print("upper:", result)

    # Python list operations
    result = python_eval("[x**2 for x in range(5)]")
    print("squares:", result)

    # Finalize Python
    python_finalize()
    print("Python finalized")
} oops {
    print("Python support not tryavailable")
    print("Build with: .\build.ps1 -WithPython")
}