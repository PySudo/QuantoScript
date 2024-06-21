require './Quanto/vars.rb'

class TOKENS
    @TOKENS = {'assignment'=> '=', 'string'=>'\'', 'comment'=>'#'}
    @TOKENS2 = {'\''=>'\'', '#'=>false}
    def self.GetTokens(*a)
        if a.size == 0
            return @TOKENS.values
        end
        out = []
        a.each do |i|
            out << @TOKENS[i]
        end
        return out
    end
    def self.GetAllTokensWithOut(except)
        @TOKENS.select{|key,value| key!=except}.values
    end
    def self.GetStopTokens(*a)
        out = []
        a.each do |i|
            out << @TOKENS2[i]
        end
        return out
    end
end

$global = ManageValues.new

class Interpreter
    def self.Execute(statements)
        statements.each_with_index do |code, line|
            next if code[0]==TOKENS.GetTokens('comment')[0]
            tokens = ManageTokens.GetAllTokens(code)
            tokens.each_with_index do |token, index|
                if TOKENS.GetTokens.include?token
                    Action.Detect(token, index, tokens, $global, line+1)
                end
            end
        end
    end
end