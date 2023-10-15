require 'fiddle'

lib = Fiddle.dlopen("/Users/saf/dev/pact-foundation/pact-php/bin/pact-ffi-lib/pact.#{RbConfig::CONFIG['SOEXT']}")

pactffi_version = Fiddle::Function.new(
  lib['pactffi_version'],
  [],
  Fiddle::TYPE_VOIDP
)

puts pactffi_version.call