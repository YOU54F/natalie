Added some Pact.io based examples

- `examples/pact*.rb`
  - make http calls
  - load ffi lib
- see `.cirrus.yml`

Binary sizes:


```sh
tree -h -L 5 tmp
[ 192]  tmp
├── [  96]  linux_arm
│   └── [  96]  binary
│       └── [  96]  pkg
│           └── [ 19M]  pact-cli
├── [  96]  linux_intel
│   └── [  96]  binary
│       └── [  96]  pkg
│           └── [ 19M]  pact-cli
├── [  96]  mac
│   └── [  96]  binary
│       └── [  96]  pkg
│           └── [6.6M]  pact-cli
└── [  96]  mac_intel
    └── [  96]  binary
        └── [  96]  pkg
            └── [7.3M]  pact-cli

13 directories, 4 files
```

On Mac linked to only system libraries:

```sh
otool -L tmp/mac_intel/binary/pkg/pact-cli  
tmp/mac_intel/binary/pkg/pact-cli:
        /usr/lib/libc++.1.dylib (compatibility version 1.0.0, current version 1600.151.0)
        /usr/lib/libSystem.B.dylib (compatibility version 1.0.0, current version 1336.0.0)
```

debian

```sh
 orb ldd tmp/linux_arm/binary/pkg/pact-cli  
        linux-vdso.so.1 (0x0000ffffa6109000)
        libcrypt.so.1 => /lib/aarch64-linux-gnu/libcrypt.so.1 (0x0000ffffa5c00000)
        libstdc++.so.6 => /lib/aarch64-linux-gnu/libstdc++.so.6 (0x0000ffffa59e0000)
        libm.so.6 => /lib/aarch64-linux-gnu/libm.so.6 (0x0000ffffa5940000)
        libgcc_s.so.1 => /lib/aarch64-linux-gnu/libgcc_s.so.1 (0x0000ffffa5900000)
        libc.so.6 => /lib/aarch64-linux-gnu/libc.so.6 (0x0000ffffa5750000)
        /lib/ld-linux-aarch64.so.1 (0x0000ffffa60cc000)
```

Alpine failing to build on a m1 mbp with orbstack in docker

```sh
** Execute build/rational_object.cpp.o
clang++ -pthread -g -Wall -Wextra -Werror -Wunused-result -Wno-missing-field-initializers -Wno-unused-parameter -Wno-unused-variable -Wno-unused-but-set-variable -Wno-unused-value -Wno-unknown-warning-option -DNAT_GC_GUARD -fPIC -I /tmp/cirrus-ci/working-dir/include -I /tmp/cirrus-ci/working-dir/ext/tm/include -I /tmp/cirrus-ci/working-dir/build -I /tmp/cirrus-ci/working-dir/build/onigmo/include -I /tmp/cirrus-ci/working-dir/build/prism/include -std=c++17 -c -o build/rational_object.cpp.o src/rational_object.cpp
rake aborted!
Command failed with status (1): [clang++ -pthread -g -Wall -Wextra -Werror ...]
/usr/local/lib/ruby/gems/3.2.0/gems/rake-13.0.6/lib/rake/file_utils.rb:67:in `block in create_shell_runner'
/usr/local/lib/ruby/gems/3.2.0/gems/rake-13.0.6/lib/rake/file_utils.rb:57:in `sh'
/tmp/cirrus-ci/working-dir/Rakefile:397:in `block in <top (required)>'
/usr/local/lib/ruby/gems/3.2.0/gems/rake-13.0.6/lib/rake/task.rb:281:in `block in execute'
/usr/local/lib/ruby/gems/3.2.0/gems/rake-13.0.6/lib/rake/task.rb:281:in `each'
/usr/local/lib/ruby/gems/3.2.0/gems/rake-13.0.6/lib/rake/task.rb:281:in `execute'
/usr/local/lib/ruby/gems/3.2.0/gems/rake-13.0.6/lib/rake/task.rb:219:in `block in invoke_with_call_chain'
/usr/local/lib/ruby/gems/3.2.0/gems/rake-13.0.6/lib/rake/task.rb:199:in `synchronize'
/usr/local/lib/ruby/gems/3.2.0/gems/rake-13.0.6/lib/rake/task.rb:199:in `invoke_with_call_chain'
/usr/local/lib/ruby/gems/3.2.0/gems/rake-13.0.6/lib/rake/task.rb:253:in `block (2 levels) in invoke_prerequisites_concurrently'
/usr/local/lib/ruby/gems/3.2.0/gems/rake-13.0.6/lib/rake/promise.rb:64:in `chore'
/usr/local/lib/ruby/gems/3.2.0/gems/rake-13.0.6/lib/rake/promise.rb:46:in `work'
/usr/local/lib/ruby/gems/3.2.0/gems/rake-13.0.6/lib/rake/thread_pool.rb:104:in `process_queue_item'
/usr/local/lib/ruby/gems/3.2.0/gems/rake-13.0.6/lib/rake/thread_pool.rb:124:in `block (2 levels) in start_thread'
Tasks: TOP => build_debug => libnatalie => primary_objects => build/file_object.cpp.o
```