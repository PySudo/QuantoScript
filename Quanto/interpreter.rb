require './Quanto/vars.rb'

class TOKENS
    @TOKENS = {'assignment'=> '=', 'string'=>'\'', 'comment'=>'#', 'list'=>'[]', 'seprator'=>','}
    def self.GetTokens(*a)
        out = []
        if a.size == 0
            @TOKENS.values.each do |i|
                if i.size > 1
                    i.chars.each do |n|
                        out << n
                    end
                else
                    out << i
                end
            end
            return out
        end
        a.each do |i|
            token = @TOKENS[i]
            if token.size > 1
                token.chars do |n|
                    out << n
                end
            else
                out << token
            end
        end
        return out
    end
end

$global = ManageValues.new

class Interpreter
    def self.Execute(statements)
        statements.each_with_index do |code, line|
            next if code.size == 0 || code[0]==TOKENS.GetTokens('comment')[0]
            tokens = ManageTokens.GetAllTokens(code, 'assignment')
            tokens.each_with_index do |token, index|
                if TOKENS.GetTokens.include?token
                    Action.Detect(token, index, tokens, $global, line+1)
                end
            end
        end
    end
end