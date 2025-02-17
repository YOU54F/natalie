require_relative '../../spec_helper'

describe "File.absolute_path?" do
  before :each do
    @abs = File.expand_path(__FILE__)
  end

  it "returns true if it's an absolute pathname" do
    File.absolute_path?(@abs).should be_true
  end

  it "returns false if it's a relative path" do
    File.absolute_path?(File.basename(__FILE__)).should be_false
  end

  it "returns false if it's a tricky relative path" do
    File.absolute_path?("C:foo\\bar").should be_false
  end

  it "does not expand '~' to a home directory." do
    File.absolute_path?('~').should be_false
  end

  it "does not expand '~user' to a home directory." do
    path = File.dirname(@abs)
    Dir.chdir(path) do
      File.absolute_path?('~user').should be_false
    end
  end

  it "calls #to_path on its argument" do
    mock = mock_to_path(File.expand_path(__FILE__))

    File.absolute_path?(mock).should be_true
  end

  platform_is_not :windows do
    it "takes into consideration the platform's root" do
      File.absolute_path?("C:\\foo\\bar").should be_false
      File.absolute_path?("C:/foo/bar").should be_false
      File.absolute_path?("/foo/bar\\baz").should be_true
    end
  end

  platform_is :windows do
    it "takes into consideration the platform path separator(s)" do
      File.absolute_path?("C:\\foo\\bar").should be_true
      File.absolute_path?("C:/foo/bar").should be_true
      File.absolute_path?("/foo/bar\\baz").should be_false
    end
  end
end

describe "File.absolute_path" do
  before :each do
    @abs = File.expand_path(__FILE__)
  end

  it "returns the argument if it's an absolute pathname" do
    NATFIXME 'Implement File.absolute_path', exception: NoMethodError, message: "undefined method `absolute_path' for class File" do
      File.absolute_path(@abs).should == @abs
    end
  end

  it "resolves paths relative to the current working directory" do
    path = File.dirname(@abs)
    Dir.chdir(path) do
      NATFIXME 'Implement File.absolute_path', exception: NoMethodError, message: "undefined method `absolute_path' for class File" do
        File.absolute_path('hello.txt').should == File.join(Dir.pwd, 'hello.txt')
      end
    end
  end

  it "does not expand '~' to a home directory." do
    NATFIXME 'Implement File.absolute_path', exception: NoMethodError, message: "undefined method `absolute_path' for class File" do
      File.absolute_path('~').should_not == File.expand_path('~')
    end
  end

  platform_is_not :windows do
    it "does not expand '~' when given dir argument" do
      NATFIXME 'Implement File.absolute_path', exception: NoMethodError, message: "undefined method `absolute_path' for class File" do
        File.absolute_path('~', '/').should == '/~'
      end
    end
  end

  it "does not expand '~user' to a home directory." do
    path = File.dirname(@abs)
    Dir.chdir(path) do
      NATFIXME 'Implement File.absolute_path', exception: NoMethodError, message: "undefined method `absolute_path' for class File" do
        File.absolute_path('~user').should == File.join(Dir.pwd, '~user')
      end
    end
  end

  it "accepts a second argument of a directory from which to resolve the path" do
    NATFIXME 'Implement File.absolute_path', exception: NoMethodError, message: "undefined method `absolute_path' for class File" do
      File.absolute_path(__FILE__, __dir__).should == @abs
    end
  end

  it "calls #to_path on its argument" do
    NATFIXME 'Implement File.absolute_path', exception: NoMethodError, message: "undefined method `absolute_path' for class File" do
      File.absolute_path(mock_to_path(@abs)).should == @abs
    end
  end
end
