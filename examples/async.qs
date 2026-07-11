# Async/await demo
from "../stdlib/async.qs" import spawn, run, result, status, run_all

# Create tasks
task1 = spawn(fn() -> "Task 1 complete")
task2 = spawn(fn() -> "Task 2 complete")
task3 = spawn(fn() -> "Task 3 complete")

print("Task IDs:", task1, task2, task3)
print("Status before run:", status(task1))

# Run all tasks
run_all()

# Check results
print("Task 1 result:", result(task1))
print("Task 2 result:", result(task2))
print("Task 3 result:", result(task3))
print("Status after run:", status(task1))

# Individual task execution
task4 = spawn(fn() -> {
    x = 10
    y = 20
    return x + y
})
run(task4)
print("Task 4 result:", result(task4))

print("Async demo complete")
