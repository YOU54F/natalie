require 'ffi'
# PKG_CONFIG_PATH=/opt/homebrew/opt/libffi/lib/pkgconfig bin/natalie examples/pact-ffi.rb
SO_EXT = RbConfig::CONFIG['SOEXT']
LIBRARY_PATH = File.expand_path("/Users/saf/dev/pact-foundation/pact-php/bin/pact-ffi-lib/pact.#{SO_EXT}", __dir__)

module PactFfi
    extend FFI::Library
    ffi_lib LIBRARY_PATH
    attach_function :pactffi_version, %i[], :pointer
end

puts PactFfi.pactffi_version.read_string