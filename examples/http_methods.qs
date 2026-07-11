# HTTP methods demo
from "../stdlib/http.qs" import get, post, put, delete, patch

# GET request
response = get("https://httpbin.org/get")
print("GET status:", response["status"])

# POST request
response = post("https://httpbin.org/post", '{"name": "QuantoScript", "version": 1}')
print("POST status:", response["status"])

# PUT request
response = put("https://httpbin.org/put", '{"update": true}')
print("PUT status:", response["status"])

# DELETE request
response = delete("https://httpbin.org/delete")
print("DELETE status:", response["status"])

# PATCH request
response = patch("https://httpbin.org/patch", '{"patch": true}')
print("PATCH status:", response["status"])
