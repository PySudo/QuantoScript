# stdlib/random.qs — Random number generation
#
# PCG32-based PRNG. Deterministic, portable, cross-platform identical sequences.
# Seed with seed(value) for reproducible results.
#
# SECURITY: This module is the only code allowed to access sys_pcg_* native functions.
# User code MUST NOT be able to call sys_pcg_next_u32 directly.
#
# NOTE: random() and random_seed() are native builtins available to all users.
# This module provides seed(), randint(), randrange(), choice, shuffle, sample.

# --- Public API ---

# Set the random seed. Two runs with the same seed produce identical sequences.
func seed(value) {
    random_seed(value)
}

# Return a random integer N such that min <= N <= max (inclusive on both ends).
# Uses sys_pcg_u32_range for correct unsigned arithmetic.
func randint(min, max) {
    if max < min {
        oops("randint: max must be >= min")
    }
    return sys_pcg_u32_range(min, max)
}

# Return a random integer N such that start <= N < stop (exclusive upper bound).
func randrange(start, stop) {
    if stop <= start {
        oops("randrange: stop must be > start")
    }
    return sys_pcg_u32_range(start, stop - 1)
}

# Return a random element from a non-empty list.
func choice(items) {
    if len(items) == 0 {
        oops("choice: list is empty")
    }
    idx = sys_pcg_u32_range(0, len(items) - 1)
    return items[idx + 1]
}

# Shuffle a list using Fisher-Yates algorithm (unbiased).
# Returns a new shuffled list (tree-walk interpreter limitation:
# cannot modify lists in-place without index assignment support).
func shuffle(items) {
    n = len(items)
    if n < 2 {
        return items
    }
    # Build shuffled result using Fisher-Yates
    remaining = []
    i = 0
    while i < n {
        remaining.append(items[i + 1])
        i = i + 1
    }
    result = []
    i = n
    while i > 0 {
        i = i - 1
        j = sys_pcg_u32_range(0, i)
        val = remaining[j + 1]
        result.append(val)
        new_remaining = []
        k = 0
        while k < len(remaining) {
            if k != j {
                new_remaining.append(remaining[k + 1])
            }
            k = k + 1
        }
        remaining = new_remaining
    }
    return result
}

# Return a new list containing k unique random elements from items.
func sample(items, k) {
    n = len(items)
    if k > n {
        oops("sample: k exceeds list length")
    }
    if k < 0 {
        oops("sample: k must be non-negative")
    }
    result = []
    remaining = []
    i = 0
    while i < n {
        remaining.append(items[i + 1])
        i = i + 1
    }
    i = 0
    while i < k {
        j = sys_pcg_u32_range(0, len(remaining) - 1)
        result.append(remaining[j + 1])
        new_remaining = []
        m = 0
        while m < len(remaining) {
            if m != j {
                new_remaining.append(remaining[m + 1])
            }
            m = m + 1
        }
        remaining = new_remaining
        i = i + 1
    }
    return result
}
