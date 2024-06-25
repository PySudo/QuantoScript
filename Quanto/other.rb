def between(start, stop, index)
    index > start and index < stop
end

def CreateSymbol(x)
    return '\'' if x=='"'
    return '"'
end

def is_int?(x)
    chars = ('0'..'9').to_a
    x.chars do |i|
        if !chars.include?i
            return false
        end
    end
    return true
end