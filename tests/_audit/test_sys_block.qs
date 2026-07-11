# Try various sys_* functions
funcs = ["sys_env", "sys_time", "sys_read", "sys_write", "sys_run", "sys_capture", "sys_json_parse", "sys_lower", "sys_upper", "sys_trim", "sys_split", "sys_exists", "sys_mkdir", "sys_remove", "sys_rename", "sys_is_dir", "sys_list_dir", "sys_cwd", "sys_chdir", "sys_exec_exists", "sys_format_time", "sys_parse_time", "sys_localtime", "sys_pcg_next_u32", "sys_random_int"]

blocked = 0
allowed = 0
for f in funcs {
    # This will fail at parse time if the function is blocked
    print("Testing: ", f)
}
print("Done")