module Natalie
  class Compiler2
    class MultipleAssignment
      def initialize(pass)
        @pass = pass
      end

      def transform(exp)
        @keyword_arg_hash_on_stack = false
        @from_side = :left
        @instructions = []
        _, *@args = exp
        while @args.any?
          arg = @from_side == :left ? @args.shift : @args.pop
          if transform_arg(arg) == :reverse
            break if @args.empty?
            arg = @args.pop
            @from_side = :right
            transform_arg(arg)
          end
        end
        @instructions << PopInstruction.new if @keyword_arg_hash_on_stack
        @instructions << PopInstruction.new
      end

      private

      def transform_arg(arg)
        if arg.is_a?(Sexp)
          case arg.sexp_type
          when :lasgn
            _, name = arg
            transform_required_arg(name)
          when :masgn
            _, names_array = arg
            transform_destructured_arg(names_array)
          when :splat
            _, name = arg
            transform_splat_arg(name)
          when :attrasgn
            _, receiver, message = arg
            transform_attr_assign_arg(receiver, message)
          else
            raise "I don't yet know how to compile #{arg.inspect}"
          end
        else
          raise "I don't yet know how to compile #{arg.inspect}"
        end
      end

      def remaining_required_args
        @args.reject { |arg| arg.is_a?(Sexp) && arg.sexp_type == :lasgn }
      end

      def transform_attr_assign_arg(receiver, message)
        shift_or_pop_next_arg
        @instructions << PushArgcInstruction.new(1)
        @instructions << @pass.transform_expression(receiver, used: true)
        @instructions << SendInstruction.new(message, receiver_is_self: false, with_block: false)
        @instructions << PopInstruction.new
      end

      def transform_destructured_arg(arg)
        @instructions << ArrayShiftInstruction.new
        sub_processor = self.class.new(@pass)
        @instructions << sub_processor.transform(arg)
      end

      def transform_keyword_arg(arg)
        _, name, default = arg
        unless @keyword_arg_hash_on_stack
          @instructions << CreateHashInstruction.new(count: 0)
          @instructions << ArrayPopWithDefaultInstruction.new
          @keyword_arg_hash_on_stack = true
        end
        if default
          @instructions << @pass.transform_expression(default, used: true)
          @instructions << HashDeleteWithDefaultInstruction.new(name)
        else
          @instructions << HashDeleteInstruction.new(name)
        end
        @instructions << variable_set(name)
      end

      def transform_optional_arg(arg)
        if remaining_required_args.any?
          # we cannot steal a value that might be needed to fulfill a required arg that follows
          # so put it back and work from the right side
          @args.unshift(arg)
          return :reverse
        end
        _, name, default_value = arg
        @instructions << @pass.transform_expression(default_value, used: true)
        shift_or_pop_next_arg_with_default
        @instructions << variable_set(name)
      end

      def transform_splat_arg(arg)
        case arg.sexp_type
        when :gasgn
          _, name = arg
          @instructions << GlobalVariableSetInstruction.new(name)
          @instructions << GlobalVariableGetInstruction.new(name)
        when :iasgn
          _, name = arg
          @instructions << InstanceVariableSetInstruction.new(name)
          @instructions << InstanceVariableGetInstruction.new(name)
        when :lasgn
          _, name = arg
          @instructions << variable_set(name)
          @instructions << VariableGetInstruction.new(name)
        when :attrasgn
          _, receiver, message = arg
          @instructions << PushArgcInstruction.new(1)
          @instructions << @pass.transform_expression(receiver, used: true)
          @instructions << SendInstruction.new(message, receiver_is_self: false, with_block: false)
        else
          raise "I don't yet know how to compile splat arg #{arg.inspect}"
        end
        :reverse
      end

      def transform_required_arg(arg)
        shift_or_pop_next_arg
        @instructions << variable_set(arg)
      end

      def shift_or_pop_next_arg
        @instructions << (@from_side == :left ? ArrayShiftInstruction.new : ArrayPopInstruction.new)
      end

      def shift_or_pop_next_arg_with_default
        if @from_side == :left
          @instructions << ArrayShiftWithDefaultInstruction.new
        else
          @instructions << ArrayPopWithDefaultInstruction.new
        end
      end

      def variable_set(name)
        VariableSetInstruction.new(name, local_only: false)
      end
    end
  end
end
