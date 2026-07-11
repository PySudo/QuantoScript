# Simple HTTP example.
# Both http:// and https:// work on Windows builds.

from "../stdlib/http.qs" import get

response = get("https://example.com/")
print("ok:", response["ok"])
print("status:", response["status"])
print("error:", response["error"])
print("body starts:", response["body"][:40])
