# examples/os.qs — Process execution with stdlib/os.qs

from "stdlib/os.qs" import run, capture, cwd, chdir, exists

# Run a command
print("=== run() ===")
rc = run("echo Hello from QuantoScript")
print("exit code:", rc)

# Capture stdout
print("\n=== capture() ===")
version = capture("echo QuantoScript v0.1.0")
print("captured:", version)

# Current working directory
print("\n=== cwd() ===")
dir = cwd()
print("cwd:", dir)

# Check if an executable exists
print("\n=== exists() ===")
has_gcc = exists("gcc")
print("gcc exists:", has_gcc)
has_fakexyz = exists("fakexyz12345")
print("fakexyz12345 exists:", has_fakexyz)

# Exit code propagation
print("\n=== exit codes ===")
rc_ok = run("echo ok")
print("echo ok exit code:", rc_ok)

print("\nAll os.qs examples complete")
