# A tiny module for import examples.
# Importing this file does not run normal lines or print anything.

APP_NAME = "QuantoScript"
VERSION = 1

func greet(name:string, ending:string="!") {
    return "hello " + name + ending
}

func double(number:int) {
    return number * 2
}

func hidden_helper() {
    return "only imported when named"
}

print("this should not print during import")
