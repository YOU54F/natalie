require_relative '../../spec_helper'

describe "Mutex#synchronize" do
  it "wraps the lock/unlock pair in an ensure" do
    m1 = Mutex.new
    m2 = Mutex.new
    m2.lock
    synchronized = false

    th = Thread.new do
      -> do
        m1.synchronize do
          synchronized = true
          m2.lock
          raise Exception
        end
      end.should raise_error(Exception)
    end

    Thread.pass until synchronized

    m1.locked?.should be_true
    m2.unlock
    th.join
    m1.locked?.should be_false
  end

  it "blocks the caller if already locked" do
    m = Mutex.new
    m.lock
    -> { m.synchronize { } }.should block_caller
  end

  it "does not block the caller if not locked" do
    m = Mutex.new
    -> { m.synchronize { } }.should_not block_caller
  end

  it "blocks the caller if another thread is also in the synchronize block" do
    NATFIXME 'Implement Queue', exception: NameError, message: 'uninitialized constant Queue' do
      m = Mutex.new
      q1 = Queue.new
      q2 = Queue.new

      t = Thread.new {
        m.synchronize {
          q1.push :ready
          q2.pop
        }
      }

      q1.pop.should == :ready

      -> { m.synchronize { } }.should block_caller

      q2.push :done
      t.join
    end
  end

  it "is not recursive" do
    m = Mutex.new

    m.synchronize do
      -> { m.synchronize { } }.should raise_error(ThreadError)
    end
  end
end
