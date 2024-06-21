def between(start, stop, index)
    index > start and index < stop
end

def CreateSymbol(x)
    return '\'' if x=='"'
    return '"'
end