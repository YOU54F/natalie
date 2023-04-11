# This file generates the specialcasing.cpp source used by Natalie.
# To regenerate, run:
#
#     ruby lib/natalie/encoding/specialcasing_gen.rb > src/encoding/specialcasing.cpp

require 'open-uri'

FLAGS = {
  'az' => 0b10000000000000000,
  'lt' => 0b100000000000000000,
  'tr' => 0b1000000000000000000,
}

def generate_function(name, data, result_index)
  puts "Value EncodingObject::#{name}(nat_int_t codepoint) {"
  puts '    switch (codepoint) {'
  data.each do |code, *rest|
    next if code.nil?
    conditions = if rest.size > 3
                   rest.pop.split
                 end
    if conditions
      with_flag = conditions.detect { |c| FLAGS[c] }
      next unless with_flag
      code = (code.to_i(16) | FLAGS[with_flag]).to_s(16)
    end
    mapping = rest[result_index].split
    if mapping.size > 1
      puts "        case 0x#{code}: return new ArrayObject({ #{mapping.map { |c| "Value::integer(0x#{c})" }.join(', ')} });"
    else
      next if mapping.first.to_s.strip == ''
      puts "        case 0x#{code}: return Value::integer(0x#{mapping.first});"
    end
  end
  puts '    }'
  puts '    return NilObject::the();'
  puts '}'
end

unless File.exist?('/tmp/SpecialCasing.txt')
  File.write(
    '/tmp/SpecialCasing.txt',
    URI.open('https://www.unicode.org/Public/UCD/latest/ucd/SpecialCasing.txt').read
  )
end

data = File.read('/tmp/SpecialCasing.txt').split(/\n/).grep_v(/^#/).map { |l| l.sub(/; #.*/, '').strip.split('; ') }

puts '// This file is auto-generated from https://www.unicode.org/Public/UCD/latest/ucd/SpecialCasing.txt'
puts '// See specialcasing_gen.rb in this repository for instructions regenerating it.'
puts '// DO NOT EDIT THIS FILE BY HAND!'
puts
puts '#include "natalie/encoding_object.hpp"'
puts '#include "natalie/nil_object.hpp"'
puts '#include "natalie/types.hpp"'
puts
puts 'namespace Natalie {'
puts
generate_function('specialcasing_lowercase', data, 0)
puts
generate_function('specialcasing_uppercase', data, 2)
puts
generate_function('specialcasing_titlecase', data, 1)
puts
puts '}'
