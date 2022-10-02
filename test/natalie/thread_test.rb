# skip-test

require_relative '../spec_helper'

describe 'thread' do
  describe '#new' do
    it 'creates a new Thread and runs it' do
      result = nil
      thread = Thread.new { result = 1000 }
      thread.should be_kind_of(Thread)
      thread.join.should == thread
      thread.join.should == thread
      thread.value.should == 1000
      thread.value.should == 1000
      result.should == 1000
    end
  end

  describe 'thread safe hash' do
    it 'works' do
      hash = {}
      threads = []
      10.times do |i|
        threads << Thread.new do
          1000.times do
            # generate some garbage
            'foo' + 'bar'
          end
          hash[i / 2] = 'hello'
        end
      end
      threads.each(&:join)
      hash.keys.sort.should == (0..4).to_a
    end
  end
end
