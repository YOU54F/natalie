module Natalie
  class Compiler
    class Args
      def initialize(pass, local_only: true, file:, line:)
        @pass = pass
        @local_only = local_only
        @file = file
        @line = line
      end

      def transform(exp)
        @from_side = :left
        @instructions = []
        _, *@args = exp
        while @args.any?
          arg = @from_side == :left ? @args.shift : @args.pop
          if transform_arg(arg) == :reverse
            @from_side = { left: :right, right: :left }.fetch(@from_side)
          end
        end
        clean_up
      end

      private

      def transform_arg(arg)
        if arg.is_a?(Sexp)
          case arg.sexp_type
          when :kwarg
            transform_keyword_arg(arg)
          when :lasgn
            clean_up_keyword_args
            if arg.size == 2
              # for-loop args look like: s(:lasgn, :x)
              transform_required_arg(arg[1])
            else
              # otherwise, they should look like: s(:lasgn, :x, s(:lit, 1))
              transform_optional_arg(arg)
            end
          when :masgn
            clean_up_keyword_args
            if arg[1].is_a?(::Prism::ArrayNode)
              # for-loop masgn looks like: s(:masgn, s(:array, ...))
              arg = arg.new(:masgn, *arg[1].elements)
            end
            transform_destructured_arg(arg)
          when :splat
            clean_up_keyword_args
            transform_splat_arg(arg[1])
          else
            raise "I don't yet know how to compile #{arg.inspect}"
          end
        elsif arg.is_a?(::Prism::OptionalParameterNode)
          clean_up_keyword_args
          transform_optional_arg(arg)
        elsif arg.is_a?(::Prism::SplatNode)
          clean_up_keyword_args
          transform_splat_arg(arg.expression)
        elsif arg.is_a?(::Prism::ArrayNode)
          clean_up_keyword_args
          transform_destructured_arg(arg)
        elsif arg.is_a?(::Prism::RequiredParameterNode)
          clean_up_keyword_args
          transform_required_arg(arg)
        elsif arg.start_with?('**')
          transform_keyword_splat_arg(arg)
        elsif arg.start_with?('*')
          clean_up_keyword_args
          transform_splat_arg(arg)
        else
          clean_up_keyword_args
          transform_required_arg(arg)
        end
      end

      def remaining_required_args
        @args.select do |arg|
          if arg.is_a?(::Prism::Node)
            arg.type == :required_parameter_node
          else
            !arg.is_a?(Sexp) && !arg.start_with?('*')
          end
        end
      end

      def remaining_keyword_args
        @args.select do |arg|
          if arg.is_a?(::Prism::Node)
            arg.type == :keyword_parameter_node
          else
            arg.is_a?(Sexp) && arg.sexp_type == :kwarg
          end
        end
      end

      def kwsplat?
        @args.any? do |arg|
          if arg.is_a?(::Prism::Node)
            arg.type == :keyword_rest_parameter_node
          else
            arg.is_a?(Symbol) && arg.start_with?('**')
          end
        end
      end

      def transform_required_arg(arg)
        shift_or_pop_next_arg
        @instructions << variable_set(arg.name)
      end

      def transform_optional_arg(arg)
        if remaining_required_args.any?
          # we cannot steal a value that might be needed to fulfill a required arg that follows
          # so put it back and work from the right side
          @args.unshift(arg)
          return :reverse
        elsif remaining_keyword_args.any? || kwsplat?
          # we cannot steal the keyword hash as if it is a regular arg,
          # so put it back and work from the right side
          @args.unshift(arg)
          return :reverse
        elsif @from_side == :right
          # optional args must be assigned from the left-to-right;
          # if we arrived here, it must be because we fulfilled all the required args on the right
          # and now we can start from the left again
          @args.push(arg)
          return :reverse
        end

        if arg.is_a?(::Prism::Node)
          name = arg.name
          default_value = arg.value
        else
          _, name, default_value = arg
        end

        if default_value&.sexp_type == :lvar && default_value[1] == name
          raise SyntaxError, "circular argument reference - #{name}"
        end

        @instructions << @pass.transform_expression(default_value, used: true)
        shift_or_pop_next_arg_with_default
        @instructions << variable_set(name)
      end

      def transform_keyword_arg(arg)
        _, name, default = arg
        move_keyword_arg_hash_from_args_array_to_stack
        if default
          @instructions << @pass.transform_expression(default, used: true)
          @instructions << HashDeleteWithDefaultInstruction.new(name)
        else
          @instructions << HashDeleteInstruction.new(name)
        end
        @instructions << variable_set(name)
      end

      def transform_destructured_arg(arg)
        @instructions << ArrayShiftInstruction.new
        @instructions << DupInstruction.new
        @instructions << ToArrayInstruction.new
        sub_processor = self.class.new(@pass, local_only: @local_only, file: @file, line: @line)
        @instructions << sub_processor.transform(arg)
        @instructions << PopInstruction.new
      end

      def transform_splat_arg(arg)
        if arg.is_a?(::Prism::Node)
          if arg.name.empty?
            :noop
          else
            @instructions << variable_set(arg.name)
            @instructions << VariableGetInstruction.new(arg.name) # TODO: could eliminate this if the *splat is the last arg
          end
        else
          name = arg.to_s.tr('*', '').to_sym
          if name.empty?
            :noop
          else
            @instructions << variable_set(name)
            @instructions << VariableGetInstruction.new(name) # TODO: could eliminate this if the *splat is the last arg
          end
        end
        :reverse
      end

      def transform_keyword_splat_arg(arg)
        name = arg[2..-1]
        move_keyword_arg_hash_from_args_array_to_stack
        if name.empty?
          :noop
        else
          @instructions << variable_set(name)
          @instructions << VariableGetInstruction.new(name) # TODO: could eliminate this if the **splat is the last arg
        end
        @has_keyword_splat = true
        :reverse
      end

      def variable_set(name)
        raise "bad var name: #{name.inspect}" unless name =~ /^[a-z_][a-z0-9_]*/
        VariableSetInstruction.new(name, local_only: @local_only)
      end

      def shift_or_pop_next_arg
        if @from_side == :left
          @instructions << ArrayShiftInstruction.new
        else
          @instructions << ArrayPopInstruction.new
        end
      end

      def shift_or_pop_next_arg_with_default
        if @from_side == :left
          @instructions << ArrayShiftWithDefaultInstruction.new
        else
          @instructions << ArrayPopWithDefaultInstruction.new
        end
      end

      def move_keyword_arg_hash_from_args_array_to_stack
        return if @keyword_arg_hash_on_stack
        @instructions << SwapInstruction.new
        @keyword_arg_hash_on_stack = true
      end

      def clean_up_keyword_args
        if @keyword_arg_hash_on_stack
          @instructions << CheckExtraKeywordsInstruction.new unless @has_keyword_splat
          @instructions << PopInstruction.new
        end
        @keyword_arg_hash_on_stack = false
      end

      def clean_up
        clean_up_keyword_args
        @instructions << PopInstruction.new
      end
    end
  end
end
