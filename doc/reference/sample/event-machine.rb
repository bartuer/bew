require 'rubygems'
require 'eventmachine'                    # => true

module FileWatcher
  def file_modified
    $path = path
    $modified = true
  end
  def file_deleted
    $path = path
    $deleted = true
  end
  def unbind
    $path = path
    $unbind = true
  end
end


EM.kqueue = true if EM.kqueue?


def test_events
  EM.run{

    # watch it
    watch = EM.watch_file('/tmp/watch', FileWatcher)
    $path = watch.path

    # modify it
    
    File.open('/tmp/watch/try', 'w'){ |f| f.puts 'hi' }
    file = File.new('/tmp/watch/try')
    
    # delete it
    EM.add_timer(5){ file.close;EM.stop}
  }

  puts "modified #{$modified.to_s} on #{$path}"                   # => nil
  puts "unwatch #{$path}"
end

test_events
