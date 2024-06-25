require './Quanto/interpreter.rb'

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

def is_string?(x)
    string = TOKENS.GetTokens('string')[0]
    if x[0] == string && x[-1] == string
        return true
    end
    return false
end

def is_list?(x)
    first, last = TOKENS.GetTokens('list')
    if x[0] == first && x[-1] == last
        return true
    end
    return false
end