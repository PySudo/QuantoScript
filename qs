require './Quanto/DataTypes.rb'
require './Quanto/tokens.rb'
require './Quanto/interpreter.rb'
require './Quanto/Action.rb'

file = open(ARGV[0])
code = file.readlines.map(&:chomp)
Interpreter.Execute code
p $global.vars