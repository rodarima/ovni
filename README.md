![Ovni logo](doc/fig/logo2.png)

The ovni project is composed of a runtime library (libovni.so), which generates
a fast binary trace, and post-processing tools such as the emulator ovniemu,
which transform the binary trace to the PRV format, suitable to be loaded in
Paraver.

The libovni.so library is licensed under MIT, while the rest of tools are GPLv3
unless otherwise stated.

For more information, read the [documentation online][doc] or take a look at the
doc/ directory. You can display the documentation in HTML by running `mkdocs
serve` from the root directory.

[doc]: https://ovni.readthedocs.io

To build ovni you would need a C compiler, MPI and cmake version 3.20 or newer.
To compile in build/ and install into `$prefix` use:

	$ mkdir build
	$ cd build
	$ cmake -DCMAKE_INSTALL_PREFIX=$prefix ..
	$ make
	$ make install

To run the tests you can run (from the build directory):

	$ make test

See cmake(1) and cmake-env-variables(7) to see more information about
the variables affecting the generation and build process.
