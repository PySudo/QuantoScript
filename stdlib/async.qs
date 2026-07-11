# Async/await helpers.
# Usage:
#   from "stdlib/async.qs" import spawn, await, run_all

# Create a new task from a lambda
func spawn(fn) {
    return sys_spawn(fn)
}

# Run a task by id
func run(task_id:int) {
    return sys_task_run(task_id)
}

# Get result from a completed task
func result(task_id:int) {
    return sys_task_result(task_id)
}

# Check task status
func status(task_id:int) {
    return sys_task_status(task_id)
}

# Run all ready tasks cooperatively
func run_all() {
    return sys_task_run_all()
}

# Convenience: spawn and run a task, then get result
func spawn_and_run(fn) {
    task_id = spawn(fn)
    run(task_id)
    return result(task_id)
}
