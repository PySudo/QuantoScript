# examples/random.qs — Random number generation examples

from "stdlib/random.qs" import seed, randint, randrange
from "stdlib/random.qs" import choice, shuffle, sample

# --- Basic random numbers ---
print("=== Random Numbers ===")
print(random())           # Float in [0.0, 1.0) - native builtin
print(randint(1, 6))     # Integer 1..6 (dice roll)
print(randrange(0, 10))  # Integer 0..9

# --- Deterministic seed ---
print("\n=== Deterministic Seed ===")
seed(42)
print(randint(1, 100))   # Always 82 with seed(42)
print(randint(1, 100))   # Always 27 with seed(42)

# Verify reproducibility
seed(42)
a = randint(1, 100)
seed(42)
b = randint(1, 100)
print("Reproducible: " + str(a == b))

# --- Choice ---
print("\n=== Choice ===")
colors = ["red", "green", "blue", "yellow"]
print("Random color: " + choice(colors))

# --- Shuffle ---
print("\n=== Shuffle ===")
cards = [1, 2, 3, 4, 5]
print("Before shuffle: " + str(cards))
shuffle(cards)
print("After shuffle:  " + str(cards))

# Verify all elements preserved
shuffle(cards)
print("Still has all: " + str(len(cards) == 5))

# --- Sample ---
print("\n=== Sample ===")
pool = [10, 20, 30, 40, 50, 60, 70, 80, 90, 100]
picked = sample(pool, 3)
print("Sampled 3: " + str(picked))
print("Original unchanged: " + str(len(pool) == 10))

# Verify no duplicates
seed(123)
picked2 = sample(pool, 5)
has_dupes = false
i = 0
while i < len(picked2) {
    j = i + 1
    while j < len(picked2) {
        if picked2[i + 1] == picked2[j + 1] {
            has_dupes = true
        }
        j = j + 1
    }
    i = i + 1
}
print("No duplicates: " + str(not has_dupes))
