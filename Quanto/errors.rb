class Error
    attr_reader :type, :message, :line
    def initialize(type, message, line=nil)
        @type = type
        @message = message
        @line = line
    end
    def ShowError
        err = "#{@type} : #{@message}"
        if @line != nil
            err += "\nline<#{@line}>"
        end
        return err
    end
end