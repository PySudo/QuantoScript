require './Quanto/errors.rb'
require './Quanto/other.rb'

class StringType
    attr_accessor :value, :type
    def initialize(string)
        @value = string
        @type = 'string'
    end
end

class ManageDataType
    def self.ExtractString(code)
        string = TOKENS.GetTokens('string')[0]
        singls = (0 ... code.length).select{|i| code[i] == string}
        if singls.size >= 2
            first, last = singls.minmax
            stringValue = code[first+1, last-1]
            return stringValue
        else
            return nil
        end
    end

    def self.DetectType(code, line)
        # code = code.strip
        string = TOKENS.GetTokens('string')[0]
        if code[0] == string && code[-1] == string
            stringValue = ExtractString(code)
            return StringType.new(stringValue)
        else
            symbol = CreateSymbol(code)
            return Error.new('Syntax', "unknown data type #{symbol}#{code}#{symbol}!", line)
        end
    end
end