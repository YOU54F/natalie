require_relative '../../spec_helper'

require 'English'

describe "English" do
  it "aliases $ERROR_INFO to $!" do
    NATFIXME '$! is rewritten at compile time, impossible to alias', exception: SpecFailedException do
      begin
        raise "error"
      rescue
        $ERROR_INFO.should_not be_nil
        $ERROR_INFO.should == $!
      end
      $ERROR_INFO.should be_nil
    end
  end

  it "aliases $ERROR_POSITION to $@" do
    NATFIXME '$@ is rewritten at compile time, impossible to alias', exception: SpecFailedException do
      begin
        raise "error"
      rescue
        $ERROR_POSITION.should_not be_nil
        $ERROR_POSITION.should == $@
      end
      $ERROR_POSITION.should be_nil
    end
  end

  it "aliases $FS to $;" do
    original = $;
    suppress_warning {$; = ","}
    $FS.should_not be_nil
    $FS.should == $;
    suppress_warning {$; = original}
  end

  it "aliases $FIELD_SEPARATOR to $;" do
    original = $;
    suppress_warning {$; = ","}
    $FIELD_SEPARATOR.should_not be_nil
    $FIELD_SEPARATOR.should == $;
    suppress_warning {$; = original}
  end

  it "aliases $OFS to $," do
    original = $,
    suppress_warning {$, = "|"}
    $OFS.should_not be_nil
    $OFS.should == $,
    suppress_warning {$, = original}
  end

  it "aliases $OUTPUT_FIELD_SEPARATOR to $," do
    original = $,
    suppress_warning {$, = "|"}
    $OUTPUT_FIELD_SEPARATOR.should_not be_nil
    $OUTPUT_FIELD_SEPARATOR.should == $,
    suppress_warning {$, = original}
  end

  it "aliases $RS to $/" do
    $RS.should_not be_nil
    $RS.should == $/
  end

  it "aliases $INPUT_RECORD_SEPARATOR to $/" do
    $INPUT_RECORD_SEPARATOR.should_not be_nil
    $INPUT_RECORD_SEPARATOR.should == $/
  end

  it "aliases $ORS to $\\" do
    original = $\
    suppress_warning {$\ = "\t"}
    $ORS.should_not be_nil
    $ORS.should == $\
    suppress_warning {$\ = original}
  end

  it "aliases $OUTPUT_RECORD_SEPARATOR to $\\" do
    original = $\
    suppress_warning {$\ = "\t"}
    $OUTPUT_RECORD_SEPARATOR.should_not be_nil
    $OUTPUT_RECORD_SEPARATOR.should == $\
    suppress_warning {$\ = original}
  end

  it "aliases $INPUT_LINE_NUMBER to $." do
    NATFIXME '$. is initialized with 0 in MRI, but is nil in Natalie, work around this limitation by reading a line', exception: SpecFailedException do
      $..should_not be_nil
    end
    File.open(__FILE__) { |f| f.readline }
    # NATFIXME: Remove the read above once this issue is fixed
    $INPUT_LINE_NUMBER.should_not be_nil
    $INPUT_LINE_NUMBER.should == $.
  end

  it "aliases $NR to $." do
    $NR.should_not be_nil
    $NR.should == $.
  end

  it "aliases $LAST_READ_LINE to $_ needs to be reviewed for spec completeness"

  it "aliases $DEFAULT_OUTPUT to $>" do
    $DEFAULT_OUTPUT.should_not be_nil
    $DEFAULT_OUTPUT.should == $>
  end

  it "aliases $DEFAULT_INPUT to $<" do
    NATFIXME 'Implement ARGF and its alias $<', exception: SpecFailedException do
      $DEFAULT_INPUT.should_not be_nil
      $DEFAULT_INPUT.should == $<
    end
  end

  it "aliases $PID to $$" do
    $PID.should_not be_nil
    $PID.should == $$
  end

  it "aliases $PID to $$" do
    $PID.should_not be_nil
    $PID.should == $$
  end

  it "aliases $PROCESS_ID to $$" do
    $PROCESS_ID.should_not be_nil
    $PROCESS_ID.should == $$
  end

  it "aliases $CHILD_STATUS to $?" do
    ruby_exe('exit 0')
    $CHILD_STATUS.should_not be_nil
    NATFIXME 'Issue with Process::Status#==', exception: SpecFailedException do
      $CHILD_STATUS.should == $?
    end
    $CHILD_STATUS.to_s.should == $?.to_s
  end

  it "aliases $LAST_MATCH_INFO to $~" do
    /c(a)t/ =~ "cat"
    $LAST_MATCH_INFO.should_not be_nil
    $LAST_MATCH_INFO.should == $~
  end

  ruby_version_is ""..."3.3" do
    it "aliases $IGNORECASE to $=" do
      $VERBOSE, verbose = nil, $VERBOSE
      begin
        $IGNORECASE.should_not be_nil
        $IGNORECASE.should == $=
      ensure
        $VERBOSE = verbose
      end
    end
  end

  it "aliases $ARGV to $*" do
    NATFIXME '$ARGV is initialized with nil', exception: SpecFailedException do
      $ARGV.should_not be_nil
    end
    $ARGV = ['a', 'b']
    # NATFIXME: Remove the assignment above once this issue is fixed
    $ARGV.should == $*
  end

  it "aliases $MATCH to $&" do
    NATFIXME '$& is rewritten at compile time, impossible to alias', exception: SpecFailedException do
      /c(a)t/ =~ "cat"
      $MATCH.should_not be_nil
      $MATCH.should == $&
    end
  end

  it "aliases $PREMATCH to $`" do
    /c(a)t/ =~ "cat"
    $PREMATCH.should_not be_nil
    $PREMATCH.should == $`
  end

  it "aliases $POSTMATCH to $'" do
    /c(a)t/ =~ "cat"
    $POSTMATCH.should_not be_nil
    $POSTMATCH.should == $'
  end

  it "aliases $LAST_PAREN_MATCH to $+" do
    /c(a)t/ =~ "cat"
    $LAST_PAREN_MATCH.should_not be_nil
    $LAST_PAREN_MATCH.should == $+
  end
end
