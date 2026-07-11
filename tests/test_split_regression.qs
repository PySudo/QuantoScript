# ============================================================================
# Regression tests for split_text_to_program()
#
# This function merges multi-line constructs into single logical lines
# and then splits them back into individual statements. These tests
# verify that statement splitting works correctly after:
#   - } (closing brace)
#   - return
#   - self.field =
#   - function calls
#   - while
#   - if
#   - repeat
#   - nested blocks
# ============================================================================

passed = 0
failed = 0

func check(name, expected, actual) {
    if expected == actual {
        passed = passed + 1
        print("PASS: " + name)
    } else {
        failed = failed + 1
        print("FAIL: " + name + " (expected " + str(expected) + ", got " + str(actual) + ")")
    }
}

# ============================================================================
# Test Group 1: return after while
# ============================================================================

class TestReturnAfterWhile {
    test_empty_loop() {
        while false {
        }
        return 1
    }

    test_single_iter() {
        i = 0
        while i < 1 {
            i = i + 1
        }
        return i
    }

    test_multi_iter() {
        i = 0
        while i < 5 {
            i = i + 1
        }
        return i
    }
}

check("return after empty while", 1, TestReturnAfterWhile().test_empty_loop())
check("return after single iter while", 1, TestReturnAfterWhile().test_single_iter())
check("return after multi iter while", 5, TestReturnAfterWhile().test_multi_iter())

# ============================================================================
# Test Group 2: return after repeat
# ============================================================================

class TestReturnAfterRepeat {
    test_empty() {
        repeat 0 {
        }
        return 1
    }

    test_once() {
        repeat 1 {
        }
        return 2
    }

    test_with_body() {
        x = 0
        repeat 3 {
            x = x + 1
        }
        return x
    }
}

check("return after empty repeat", 1, TestReturnAfterRepeat().test_empty())
check("return after repeat 1", 2, TestReturnAfterRepeat().test_once())
check("return after repeat with body", 3, TestReturnAfterRepeat().test_with_body())

# ============================================================================
# Test Group 3: self.field assignment after while
# ============================================================================

class TestSelfFieldAfterWhile {
    init() {
        self.x = 0
        self.y = 0
    }

    test_empty_loop() {
        while false {
        }
        self.x = 42
    }

    test_with_loop() {
        i = 0
        while i < 3 {
            i = i + 1
        }
        self.x = i
    }

    test_multiple_fields() {
        while false {
        }
        self.x = 10
        self.y = 20
    }
}

t1 = TestSelfFieldAfterWhile()
t1.test_empty_loop()
check("self.field after empty while", 42, t1.x)

t2 = TestSelfFieldAfterWhile()
t2.test_with_loop()
check("self.field after while with body", 3, t2.x)

t3 = TestSelfFieldAfterWhile()
t3.test_multiple_fields()
check("multiple self.fields after while", 10, t3.x)
check("second self.field", 20, t3.y)

# ============================================================================
# Test Group 4: Function calls after while
# ============================================================================

func helper() {
    return 100
}

class TestFuncCallAfterWhile {
    test_empty() {
        while false {
        }
        return helper()
    }

    test_with_body() {
        i = 0
        while i < 2 {
            i = i + 1
        }
        return helper() + i
    }
}

check("function call after empty while", 100, TestFuncCallAfterWhile().test_empty())
check("function call after while with body", 102, TestFuncCallAfterWhile().test_with_body())

# ============================================================================
# Test Group 5: if after while
# ============================================================================

class TestIfAfterWhile {
    test_empty() {
        while false {
        }
        if true {
            return 10
        }
        return 20
    }

    test_with_body() {
        i = 0
        while i < 1 {
            i = i + 1
        }
        if i == 1 {
            return 30
        }
        return 40
    }

    test_maybe() {
        while false {
        }
        if false {
            return 50
        } maybe true {
            return 60
        }
        return 70
    }
}

check("if after empty while", 10, TestIfAfterWhile().test_empty())
check("if after while with body", 30, TestIfAfterWhile().test_with_body())
check("maybe after while", 60, TestIfAfterWhile().test_maybe())

# ============================================================================
# Test Group 6: repeat after while
# ============================================================================

class TestRepeatAfterWhile {
    test_simple() {
        while false {
        }
        x = 0
        repeat 3 {
            x = x + 1
        }
        return x
    }
}

check("repeat after while", 3, TestRepeatAfterWhile().test_simple())

# ============================================================================
# Test Group 7: Nested blocks
# ============================================================================

class TestNestedBlocks {
    test_while_in_while() {
        i = 0
        while i < 3 {
            j = 0
            while j < 2 {
                j = j + 1
            }
            i = i + 1
        }
        return i
    }

    test_if_in_while() {
        i = 0
        while i < 5 {
            if i == 3 {
                break
            }
            i = i + 1
        }
        return i
    }

    test_while_in_if() {
        if true {
            i = 0
            while i < 4 {
                i = i + 1
            }
            return i
        }
        return 0
    }
}

check("nested while loops", 3, TestNestedBlocks().test_while_in_while())
check("if break in while", 3, TestNestedBlocks().test_if_in_while())
check("while inside if", 4, TestNestedBlocks().test_while_in_if())

# ============================================================================
# Test Group 8: Complex combinations
# ============================================================================

class TestComplex {
    init() {
        self.result = 0
        self.n = 10
    }

    test_full() {
        c = 0
        i = 0

        while i < self.n {
            c = c + 1
            i = i + 1
        }

        self.result = c
        return c
    }

    test_chained() {
        while false {
        }
        self.result = 1
        self.result = self.result + 1
        self.result = self.result * 10
        return self.result
    }

    test_with_helper() {
        i = 0
        while i < 3 {
            i = i + 1
        }
        return helper() + i
    }
}

t4 = TestComplex()
check("complex while with self assignment", 10, t4.test_full())
check("complex result", 10, t4.result)

t5 = TestComplex()
check("chained assignments after while", 20, t5.test_chained())

check("while with helper call", 103, TestComplex().test_with_helper())

# ============================================================================
# Test Group 9: Edge cases
# ============================================================================

class TestEdgeCases {
    test_single_line_while() {
        while false { }
        return 1
    }

    test_empty_method_after_while() {
        while false {
        }
        return 2
    }

    test_return_before_while() {
        return 3
        while false {
        }
    }
}

check("single line while", 1, TestEdgeCases().test_single_line_while())
check("empty method after while", 2, TestEdgeCases().test_empty_method_after_while())
check("return before while (unreachable)", 3, TestEdgeCases().test_return_before_while())

# ============================================================================
# Summary
# ============================================================================

print("")
print("===============================")
print("Results: " + str(passed) + " passed, " + str(failed) + " failed")
print("===============================")

if failed > 0 {
    print("SOME TESTS FAILED!")
    print("")
    print("Known VM issues (AST compiler bugs):")
    print("  - Function calls after while loops return null")
    print("  - If/maybe after while loops skip branches")
    print("  - Break in while loops doesn't work correctly")
    print("  - While inside if doesn't execute")
} else {
    print("ALL TESTS PASSED!")
}
