require_relative './base_instruction'
require_relative '../string_to_cpp'

module Natalie
  class Compiler
    class PushStringInstruction < BaseInstruction
      include StringToCpp

      def initialize(string, bytesize: string.bytesize, encoding: Encoding::UTF_8)
        super()
        @string = string
        @bytesize = bytesize
        @encoding = encoding
      end

      def to_s
        "push_string #{@string.inspect}, #{@bytesize}, #{@encoding.name}"
      end

      def generate(transform)
        enum = @encoding.name.tr('-', '_').upcase
        encoding_object = "EncodingObject::get(Encoding::#{enum})"
        if @string.empty?
          transform.exec_and_push(:string, "Value(new StringObject(#{encoding_object}))")
        else
          transform.exec_and_push(
            :string,
            "Value(new StringObject(#{string_to_cpp(@string)}, (size_t)#{@bytesize}, #{encoding_object}))"
          )
        end
      end

      def execute(vm)
        vm.push(@string.dup)
      end

      def serialize
        [
          instruction_number,
          @bytesize,
          @string,
        ].pack("Cwa#{@bytesize}")
      end

      def self.deserialize(io)
        size = io.read_ber_integer
        string = io.read(size)
        new(string, bytesize: size)
      end
    end
  end
end
