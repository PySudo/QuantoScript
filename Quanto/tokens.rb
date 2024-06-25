require './Quanto/errors.rb'
require './Quanto/interpreter.rb'
require './Quanto/other.rb'

class ManageTokens
    def self.DeleteComments(lst)
        comment, string = TOKENS.GetTokens('comment', 'string')
        indexs = (0...lst.length).select{|i| lst[i].include?comment}
        indexs.each do |i|
            element = lst[i]
            stringIndexs = (0...element.length).select{|n| element[n]==string}
            commentIndexs = (0...element.length).select{|n| element[n]==comment}
            first, last = stringIndexs.minmax
            outOfString = commentIndexs.select{|i| !between(first, last, i)}
            lst[i] = element[0...outOfString.sort[0]].strip
        end
        return lst
    end

    def self.GetAllTokens(statement, *a)
        cache = ''
        out = []
        isString = false
        statement.chars.each do |i|
            unless TOKENS.GetTokens(*a).include?i
                cache += i
            else
                out << cache.strip if cache.strip.size >= 1
                out << i
                cache = ''
            end
        end
        out << cache.strip if cache.strip.size >= 1
        return DeleteComments(out)
    end
end