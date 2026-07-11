# Time helpers.

# Get current timestamp (seconds since epoch)
func now() {
    return sys_time()
}

# Get local time as a map with year/month/day/hour/min/sec/wday/yday
func localtime() {
    return sys_localtime()
}

# Format a timestamp using strftime format
# Example: format(now(), "%Y-%m-%d %H:%M:%S")
func format(timestamp:int, fmt:string) {
    return sys_format_time(timestamp, fmt)
}

# Parse a time string
# Example: parse("2024-01-15 10:30:00", "%Y-%m-%d %H:%M:%S")
func parse(text:string, fmt:string) {
    return sys_parse_time(text, fmt)
}

# Get current year
func year() {
    t = localtime()
    return t["year"]
}

# Get current month (1-12)
func month() {
    t = localtime()
    return t["month"]
}

# Get current day (1-31)
func day() {
    t = localtime()
    return t["day"]
}

# Get current hour (0-23)
func hour() {
    t = localtime()
    return t["hour"]
}

# Get current minute (0-59)
func minute() {
    t = localtime()
    return t["min"]
}

# Get current second (0-59)
func second() {
    t = localtime()
    return t["sec"]
}

# Get day of week (0=Sunday, 6=Saturday)
func weekday() {
    t = localtime()
    return t["wday"]
}

# Get day of year (0-365)
func yearday() {
    t = localtime()
    return t["yday"]
}

# Get formatted current time
func now_str() {
    return format(now(), "%Y-%m-%d %H:%M:%S")
}

# Get formatted current date
func today_str() {
    return format(now(), "%Y-%m-%d")
}

# Calculate difference in seconds between two timestamps
func diff_seconds(ts1:int, ts2:int) {
    if ts1 > ts2 {
        return ts1 - ts2
    }
    return ts2 - ts1
}

# Add seconds to a timestamp
func add_seconds(timestamp:int, seconds:int) {
    return timestamp + seconds
}

# Add minutes to a timestamp
func add_minutes(timestamp:int, minutes:int) {
    return timestamp + minutes * 60
}

# Add hours to a timestamp
func add_hours(timestamp:int, hours:int) {
    return timestamp + hours * 3600
}

# Add days to a timestamp
func add_days(timestamp:int, days:int) {
    return timestamp + days * 86400
}
