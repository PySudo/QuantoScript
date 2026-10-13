from "stdlib/json.qs" import parse

# \uXXXX escapes must decode to real Unicode characters, not literal "uXXXX".
persian = parse('{"text": "\u0633\u0644\u0627\u0645"}')
print(persian["text"])

# A UTF-16 surrogate pair must combine into a single code point.
emoji = parse('{"text": "\uD83D\uDE00"}')
print(emoji["text"])

# Standard JSON escapes still decode.
escapes = parse('{"text": "A\\tB\\nC \\"q\\" \\\\ end"}')
print(escapes["text"])
