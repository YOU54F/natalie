require 'natalie/inline'
require 'openssl.cpp'

__ld_flags__ '-lcrypto'
__ld_flags__ '-lssl'

# We have some circular dependencies here: our digest implementations are simply aliases to OpenSSL::Digest classes,
# but OpenSSL::Digest depends on ::Digest::Instance. So define this one in openssl.rb instead of digest.rb.
module Digest
  module Instance
    def update(_)
      raise "#{self} does not implement update()"
    end
    alias << update

    def new
      OpenSSL::Digest.new(name)
    end

    def length
      digest_length
    end

    def size
      digest_length
    end

    def digest!(...)
      digest(...).tap { reset }
    end

    def hexdigest!(...)
      hexdigest(...).tap { reset }
    end

    def self.included(klass)
      klass.define_singleton_method(:file) do |file, *args|
        file = file.to_str if !file.is_a?(String) && file.respond_to?(:to_str)
        raise TypeError, 'TODO' unless file.is_a?(String)
        new(*args, File.read(file))
      end
    end
  end
end

module OpenSSL
  class OpenSSLError < StandardError; end

  def self.secure_compare(a, b)
    sha1_a = Digest.digest('SHA1', a)
    sha1_b = Digest.digest('SHA1', b)
    fixed_length_secure_compare(sha1_a, sha1_b) && a == b
  end

  module ASN1
    __constant__('EOC', 'int', 'V_ASN1_EOC')
    __constant__('BOOLEAN', 'int', 'V_ASN1_BOOLEAN')
    __constant__('INTEGER', 'int', 'V_ASN1_INTEGER')
    __constant__('BIT_STRING', 'int', 'V_ASN1_BIT_STRING')
    __constant__('OCTET_STRING', 'int', 'V_ASN1_OCTET_STRING')
    __constant__('NULL', 'int', 'V_ASN1_NULL')
    __constant__('OBJECT', 'int', 'V_ASN1_OBJECT')
    __constant__('OBJECT_DESCRIPTOR', 'int', 'V_ASN1_OBJECT_DESCRIPTOR')
    __constant__('EXTERNAL', 'int', 'V_ASN1_EXTERNAL')
    __constant__('REAL', 'int', 'V_ASN1_REAL')
    __constant__('ENUMERATED', 'int', 'V_ASN1_ENUMERATED')
    __constant__('UTF8STRING', 'int', 'V_ASN1_UTF8STRING')
    __constant__('SEQUENCE', 'int', 'V_ASN1_SEQUENCE')
    __constant__('SET', 'int', 'V_ASN1_SET')
    __constant__('NUMERICSTRING', 'int', 'V_ASN1_NUMERICSTRING')
    __constant__('PRINTABLESTRING', 'int', 'V_ASN1_PRINTABLESTRING')
    __constant__('T61STRING', 'int', 'V_ASN1_T61STRING')
    __constant__('VIDEOTEXSTRING', 'int', 'V_ASN1_VIDEOTEXSTRING')
    __constant__('IA5STRING', 'int', 'V_ASN1_IA5STRING')
    __constant__('UTCTIME', 'int', 'V_ASN1_UTCTIME')
    __constant__('GENERALIZEDTIME', 'int', 'V_ASN1_GENERALIZEDTIME')
    __constant__('GRAPHICSTRING', 'int', 'V_ASN1_GRAPHICSTRING')
    __constant__('ISO64STRING', 'int', 'V_ASN1_ISO64STRING')
    __constant__('GENERALSTRING', 'int', 'V_ASN1_GENERALSTRING')
    __constant__('UNIVERSALSTRING', 'int', 'V_ASN1_UNIVERSALSTRING')
    __constant__('BMPSTRING', 'int', 'V_ASN1_BMPSTRING')
  end

  module Random
    __bind_static_method__ :random_bytes, :OpenSSL_Random_random_bytes
  end

  class Cipher
    class CipherError < OpenSSLError; end

    def random_iv
      self.iv = Random.random_bytes(iv_len)
    end

    def random_key
      self.key = Random.random_bytes(key_len)
    end
  end

  class Digest
    include ::Digest::Instance
    attr_reader :name

    def self.digest(name, data)
      new(name).digest(data)
    end

    def self.base64digest(name, data)
      new(name).base64digest(data)
    end

    def self.hexdigest(name, data)
      new(name).hexdigest(data)
    end

    def base64digest(...)
      [digest(...)].pack('m0')
    end

    def hexdigest(...)
      digest(...).unpack1('H*')
    end

    def self.const_missing(name)
      normalized_name = new(name.to_s).name
      raise if name.to_s != normalized_name
      klass = Class.new(self) do
        define_method(:initialize) { |*args| super(normalized_name, *args) }
        define_singleton_method(:digest) { |*args| Digest.digest(normalized_name, *args) }
        define_singleton_method(:base64digest) { |*args| Digest.base64digest(normalized_name, *args) }
        define_singleton_method(:hexdigest) { |*args| Digest.hexdigest(normalized_name, *args) }
      end
      const_set(name, klass)
      klass
    rescue StandardError
      super
    end
  end

  class HMAC
    def self.hexdigest(digest, key, data)
      digest(digest, key, data).unpack1('H*')
    end
  end

  module SSL
    class SSLContext
      __bind_method__ :initialize, :OpenSSL_SSL_SSLContext_initialize
    end

    class SSLSocket
      attr_reader :context, :io

      __bind_method__ :initialize, :OpenSSL_SSL_SSLSocket_initialize
      __bind_method__ :close, :OpenSSL_SSL_SSLSocket_close
      __bind_method__ :connect, :OpenSSL_SSL_SSLSocket_connect
      __bind_method__ :read, :OpenSSL_SSL_SSLSocket_read
      __bind_method__ :write, :OpenSSL_SSL_SSLSocket_write
    end
  end

  module KDF
    class KDFError < OpenSSLError; end
  end

  module X509
    class NameError < OpenSSLError; end

    class Name
      OBJECT_TYPE_TEMPLATE = {
        'C'               => ASN1::PRINTABLESTRING,
        'countryName'     => ASN1::PRINTABLESTRING,
        'serialNumber'    => ASN1::PRINTABLESTRING,
        'dnQualifier'     => ASN1::PRINTABLESTRING,
        'DC'              => ASN1::IA5STRING,
        'domainComponent' => ASN1::IA5STRING,
        'emailAddress'    => ASN1::IA5STRING
      }.tap { |hash| hash.default = ASN1::UTF8STRING }.freeze

      __constant__('COMPAT', 'int', 'XN_FLAG_COMPAT')
      __constant__('RFC2253', 'int', 'XN_FLAG_RFC2253')
      __constant__('ONELINE', 'int', 'XN_FLAG_ONELINE')
      __constant__('MULTILINE', 'int', 'XN_FLAG_MULTILINE')

      __bind_method__ :initialize, :OpenSSL_X509_Name_initialize
      __bind_method__ :add_entry, :OpenSSL_X509_Name_add_entry
      __bind_method__ :to_a, :OpenSSL_X509_Name_to_a
      __bind_method__ :to_s, :OpenSSL_X509_Name_to_s

      class << self
        def parse_openssl(str, template = OBJECT_TYPE_TEMPLATE)
          ary = if str.start_with?('/')
                  str.split('/')[1..]
                else
                  str.split(/,\s*/)
                end
          ary = ary.map { |e| e.split('=') }
          raise TypeError, 'invalid OpenSSL::X509::Name input' unless ary.all? { |e| e.size == 2 }
          new(ary, template)
        end

        alias parse parse_openssl
      end
    end
  end
end
