![microblx logo](/docs/user/_static/microblx-logo.png)

microblx: hard realtime function blocks
=======================================

[![Build status](https://travis-ci.org/kmarkus/microblx.svg?branch=master)](https://travis-ci.org/kmarkus/microblx)
[![Documentation status](https://readthedocs.org/projects/microblx/badge/?version=latest)](http://microblx.readthedocs.io/?badge=latest)

Microblx is a lightweight and hard real-time safe function block
framework. Main use-cases are building embedded control systems or
signal processing applications.

Main features:

- **minimal**: small core in C with few dependencies
- **hard real-time safe**: no run-time allocations
- **extensible**: easy to add new communications channels (called interaction blocks)
- simple yet expressive **textual language** to specify
  block compositions and launch these using `ubx_launch`
- **standard components**:
  - communication: lock free buffers, POSIX message queues,
  - computation: PID controller, ramps, ...
  - generic triggers including built in latency profiling
  - real-time safe logging
- various **cmdline tools**
  - log client (`ubx-log`)
  - block stub code generation (`ubx-genblock`)
  - inspection and doc generation (`ubx-modinfo`)

Documentation
-------------

The documentation is [here](https://microblx.readthedocs.io) or can be
built locally (requires sphinx to be installed):

```bash
$ cd docs/
$ make html
Running Sphinx v1.8.5
...
```

There is also a [ChangeLog](/ChangeLog.md) which summarizes API
changes or important features and a high-level
[roadmap](/docs/dev/roadmap.md).

Getting help
------------

Please feel free to ask questions or report problems on the microblx
mailing list:

<https://groups.google.com/forum/#!forum/microblx>

It is possible to subscribe by email by sending a mail to
`microblx+subscribe@googlegroups.com`

Contributing
------------

Contributions are very welcome. Please check that the following
requirements are met:

- contributions must conform to the Linux kernel [coding
  style](https://www.kernel.org/doc/html/latest/process/coding-style.html).

- before submitting, please run the the tests (`./run_tests.sh` after
  installing) and static checking (`make cppcheck`, shouldn't output
  anything).

- the preferred ways of submitting patches is via the mailing list. If
  you must, a github merge request is OK too.

- please don't forget to add a line
  `Signed-off-by: Random J Developer <random@developer.example.org>`
  to certify conformance with the *Developer's Certificate of Origin
  1.1* below.

### Developer's Certificate of Origin 1.1

By making a contribution to this project, I certify that:

(a) The contribution was created in whole or in part by me and I
    have the right to submit it under the open source license
    indicated in the file; or

(b) The contribution is based upon previous work that, to the best
    of my knowledge, is covered under an appropriate open source
    license and I have the right under that license to submit that
    work with modifications, whether created in whole or in part
    by me, under the same open source license (unless I am
    permitted to submit under a different license), as indicated
    in the file; or

(c) The contribution was provided directly to me by some other
    person who certified (a), (b) or (c) and I have not modified
    it.

(d) I understand and agree that this project and the contribution
    are public and that a record of the contribution (including all
    personal information I submit with it, including my sign-off) is
    maintained indefinitely and may be redistributed consistent with
    this project or the open source license(s) involved.

License
-------

See COPYING. The microblx core is licensed under the weak copyleft
Mozilla Public License Version 2.0 (MPL-2.0). Standard blocks are
mostly licensed under the permissive BSD 3-Clause "New" or "Revised"
License or also under the MPLv2.

It boils down to the following. Use microblx as you wish in free and
proprietary applications. You can distribute binary function blocks as
modules. Only if you make changes to the core files (mostly the files
in libubx/) library), and distribute these, then you are required to
release these under the conditions of the MPL-2.0.

Acknowledgement
---------------

Microblx is considerably inspired by the OROCOS Real-Time
Toolkit. Other influences are the IEC standards covering function
block IEC-61131 and IEC-61499.

This work was supported by the European FP7 projects RoboHow
(FP7-ICT-288533), BRICS (FP7- ICT-231940), Rosetta (FP7-ICT-230902),
Pick-n-Pack (FP7-NMP-311987) and euRobotics (FP7-ICT-248552).
