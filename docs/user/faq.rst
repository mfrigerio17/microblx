Frequently asked questions
==========================

meta-microblx yocto layer: luajit build fails
---------------------------------------------

``luajit`` fails with the following message:

.. code:: sh
	  
   arm-poky-linux-gnueabi-gcc  -mfpu=neon -mfloat-abi=hard -mcpu=cortex-a8 -fstack-protector-strong  -D_FORTIFY_SOURCE=2 -Wformat -Wformat-security -Werror=format-security --sysroot=/build/bbblack-zeus/build/tmp/work/cortexa8hf-neon-poky-linux-gnueabi/luajit/2.0.5+gitAUTOINC+02b521981a-r0/recipe-sysroot -fPIC   -Wall   -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE -U_FORTIFY_SOURCE  -DLUA_ROOT=\"/usr\" -DLUA_MULTILIB=\"lib\" -fno-stack-protector  -O2 -pipe -g -feliminate-unused-debug-types -fmacro-prefix-map=/build/bbblack-zeus/build/tmp/work/cortexa8hf-neon-poky-linux-gnueabi/luajit/2.0.5+gitAUTOINC+02b521981a-r0=/usr/src/debug/luajit/2.0.5+gitAUTOINC+02b521981a-r0                      -fdebug-prefix-map=/build/bbblack-zeus/build/tmp/work/cortexa8hf-neon-poky-linux-gnueabi/luajit/2.0.5+gitAUTOINC+02b521981a-r0=/usr/src/debug/luajit/2.0.5+gitAUTOINC+02b521981a-r0                      -fdebug-prefix-map=/build/bbblack-zeus/build/tmp/work/cortexa8hf-neon-poky-linux-gnueabi/luajit/2.0.5+gitAUTOINC+02b521981a-r0/recipe-sysroot=                      -fdebug-prefix-map=/build/bbblack-zeus/build/tmp/work/cortexa8hf-neon-poky-linux-gnueabi/luajit/2.0.5+gitAUTOINC+02b521981a-r0/recipe-sysroot-native=  -c -o lj_obj_dyn.o lj_obj.c
   In file included from /usr/include/bits/errno.h:26,
                    from /usr/include/errno.h:28,
                    from host/buildvm.h:13,
                    from host/buildvm_fold.c:6:
   /usr/include/linux/errno.h:1:10: fatal error: asm/errno.h: No such file or directory
       1 | #include <asm/errno.h>
         |          ^~~~~~~~~~~~~
   compilation terminated.

This solution is to install ``gcc-multilib`` on the build host.


"undefined symbol" errors when loading modules
----------------------------------------------

Try rerunning ldconfig (``sudo ldconfig``). It seems there is an
libtool issue https://bugs.debian.org/cgi-bin/bugreport.cgi?bug=684981
that makes this necessary under certain circumstances (?).


fix “error object is not a string”
----------------------------------

Note that this has been fixed in commit ``be63f6408bd4d``.

This is most of the time happens when the ``strict`` module being loaded
(also indirectly, e.g. via ubx.lua) in a luablock. It is caused by the C
code looking up a non-existing global hook function. Solution: either
define all hooks or disable the strict module for the luablock.


``blockXY``.so or ``liblfds611.so.0``: cannot open shared object file: No such file or directory
------------------------------------------------------------------------------------------------

There seems to be a `bug
<https://bugs.debian.org/cgi-bin/bugreport.cgi?bug=684981>`_ in some
versions of libtool which leads to the ld cache not being updated. You
can manually fix this by running

.. code:: sh

	$ sudo ldconfig


Often this means that the location of the shared object file is not in
the library search path. If you installed to a non-standard location,
try adding it to ``LD_LIBRARY_PATH``, e.g.

.. code:: sh

   $ export LD_LIBRARY_PATH=/usr/local/lib/

It would be better to install stuff in a standard location such as
``/usr/local/``.

Real-time priorities
--------------------

To run with realtime priorities, give the luajit binary ``cap_sys_nice``
capabilities, e.g:

.. code:: sh

   $ sudo setcap cap_sys_nice+ep `which luajit`

Note that this will grant these capabilities to luajit binary in
`PATH`. If you don't want to do this system-wide, then specify a
different version!

My script immedately crashes/finishes
-------------------------------------

This can have several reasons:

-  You forgot the ``-i`` option to ``luajit``: in that case the script
   is executed and once completed will immedately exit. The system will
   be shut down / cleaned up rather rougly.

-  You ran the wrong Lua executable (e.g. a standard Lua instead of
   ``luajit``).

If none of this works, see the following topic.


Debugging segfaults
-------------------

One of the best ways to debug crashes is using gdb and the core dump
file:

.. code:: sh
	  
   # enable core dumps
   $ ulimit -c unlimited
   $ gdb luajit
   ...
   (gdb) core-file core
   ...
   (gdb) bt


Sometimes, running gdb directly on the processes produces better
results than post-mortem coredumps. For example, to run the pid
example with gdb attached:

.. code:: sh

   $ cd /usr/local/share/ubx/examples/usc/pid
   $ gdb luajit --args luajit `which ubx-launch` -c pid_test.usc,ptrig_nrt.usc
   GNU gdb (Debian 9.1-2) 9.1
   ...
   Reading symbols from luajit...
   (No debugging symbols found in luajit)
   (gdb) run
   Starting program: /usr/bin/luajit /usr/local/bin/ubx-launch -c pid_test.usc,ptrig_nrt.usc
   [Thread debugging using libthread_db enabled]
   Using host libthread_db library "/lib/x86_64-linux-gnu/libthread_db.so.1".
   merging ptrig_nrt.usc into pid_test.usc
   core_prefix: /usr/local
   prefixes:    /usr, /usr/local
   [New Thread 0x7ffff7871700 (LWP 2831757)]
   ...
