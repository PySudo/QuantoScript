require './Quanto/DataTypes.rb'
require './Quanto/errors.rb'
require './Quanto/other.rb'

class ActionType
    attr_reader :type, :datatype
    def initialize(type, dataType)
        @type = type
        @datatype = dataType
    end
end

class Action
    def self.AssignmentRule(varName, line, scopeInstance)
        if varName == varName.upcase and scopeInstance.vars.keys.include?varName
            return Error.new('DefinitionError', "\"#{varName}\" already exists in this scope, you can't change this variable value.", line)
        end
        letters = (65..90).map{|i| i.chr}.join('')
        letters << (97..122).map{|i| i.chr}.join('')
        if letters.include?varName[0]
            letters << (48..57).map{|i| i.chr}.join('')
            letters << '_'
            varName[1..].chars.each do |n|
                unless letters.include?n
                    symbol = CreateSymbol(n)
                    return Error.new('Syntax', "You can't use #{symbol}#{n}#{symbol} in the variable name!", line)
                end
            end
        else
            symbol = CreateSymbol(varName[0])
            return Error.new('Syntax', "You can't use #{symbol}#{varName[0]}#{symbol} to start a variable name.", line)
        end
    end
    def self.Detect(token, index, tokens, scopeInstance, line)
        # puts "token for detect type = #{token}"
        case token
        when TOKENS.GetTokens('assignment')[0]
            check = AssignmentRule(tokens[index-1], line, scopeInstance)
            if check.is_a?Error
                puts check.ShowError
                exit 1
            end
            detectType = ManageDataType.DetectType(tokens[index+1], line)
            if detectType.is_a?Error
                puts detectType.ShowError
                exit 1
            end
            if detectType.type == 'list'
                scopeInstance.vars[tokens[index-1]] = []
                detectType.value.each do |obj|
                    scopeInstance.vars[tokens[index-1]] << obj.value
                end
            else
                scopeInstance.vars[tokens[index-1]] = detectType.value
            end
                return ActionType.new('Assignment', detectType.type)
        # when ','
            
        # else
        #     symbol = CreateSymbol(token)
        #     return Error.new('Syntax', "unexpected #{symbol}#{token}#{symbol}", line)
            
        end
    end
end