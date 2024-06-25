require './Quanto/errors.rb'
require './Quanto/other.rb'
require './Quanto/tokens.rb'
require './Quanto/interpreter.rb'

class StringType
    attr_accessor :value, :type
    def initialize(string)
        @value = string
        @type = 'string'
    end
end

class IntegerType
    attr_accessor :value, :type
    def initialize(integer)
        @value = integer.to_i
        @type = 'integer'
    end
end

class ListType
    attr_accessor :value, :type
    def initialize(list, line)
        @value = CreateList(list, line)
        @type = 'list'
    end
    def GetElements(code)
        ignore = TOKENS.GetTokens
        tokens = ManageTokens.GetAllTokens(code, 'list', 'seprator')
        elements = []
        tokens.each do |i|
            unless ignore.include?i
                elements << i
            end
        end
        return elements
    end
    def CreateList(code, line)
        elements = GetElements code
        value = []
        elements.each do |element|
            value << ManageDataType.DetectType(element, line)
        end
        return value
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
        if is_string?(code)
            stringValue = ExtractString(code)
            return StringType.new(stringValue)
        elsif is_int?(code)
            return IntegerType.new(code)
        elsif is_list?(code)
            return ListType.new(code, line)
        else
            symbol = CreateSymbol(code)
            return Error.new('Syntax', "unknown data type #{symbol}#{code}#{symbol}!", line)
        end
    end
end