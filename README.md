# QuantoScript
- nothing for now...

# DataTypes

## integer
- Create a variable with value 10 :
```qs
myInt = 10
```

## string
- Syntax
```qs
'its a string'
```
if you want to use single quote in strings just put it in string , don't need to use \ in string , for example :
```qs
myString = 'it's a string'
```
now `myString` have this value : "i use ' in string."


# variables
- You can store any data type in a variable.
- You can't use "_" as start of a variable name.

## Syntax :
```qs
var_name = dataType
```

- DEFINE variables :
```qs
VAR = 'This is a DEFINE variable'
```
You should to write all variable letters in capital letters for DEFINE.

# comments
- for comments you can use `#` (optional).
 for example :

```qs
VAR = 'This is a string' # This is a DEFINE variable
myString = 'This is a changable variable.'

This is a comment.
But its better to use "#" for comments...
```
if you want to set comment to a empty line that optional to use `#`. for example :
```qs
This is a comment
```
and
```qs
# This is a comment
```
these same , but if you want to set a comment in a variable line or... you should to use `#` like this :
```qs
myVar = 'i have a comment in this line' # This is a comment
```
But you can't use comments like this :
```qs
myVar = 'i have a comment in this line' This is a comment
```

## usage
```bash
ruby qs FILE.qs
```
