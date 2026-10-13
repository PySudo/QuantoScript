# Tests for stdlib/core.qs: parse_env and load_env

from "stdlib/core.qs" import parse_env, load_env
from "stdlib/fs.qs" import write, remove

# --- parse_env: pure parsing ---
text = "# a comment\nGREETING=hello\nTOKEN=abc=def\nSPACED = hello world\nEMPTY=\n\n"
cfg = parse_env(text)

print("keys:", cfg.keys())
print("GREETING:", cfg["GREETING"])
print("TOKEN:", cfg["TOKEN"])
print("SPACED:", cfg["SPACED"])
print("EMPTY:", "[", cfg["EMPTY"], "]")

# --- load_env: reading a real file ---
write("qs_load_env_test.env", "NAME=Quanto\nPORT=8080\n")
file_cfg = load_env("qs_load_env_test.env")
print("file NAME:", file_cfg["NAME"])
print("file PORT:", file_cfg["PORT"])

# A missing file yields an empty map.
print("missing keys:", load_env("qs_load_env_missing.env").keys())

remove("qs_load_env_test.env")
