# ATS2-zigtools

**Russian:** [README.ru.md](README.ru.md)

A port of the [Applied Type System 2](https://ats-lang.sourceforge.net/) (Postiats 0.4.2) compiler and standard library.
All you need to build the compiler and standard library is `Zig 0.17.0` and the `zig build` command.  
Unlike the original, the standard library has been patched to run natively on Windows without a dependency on Cygwin.  

## Building and Testing

Requirements: `Zig 0.17.0` available in your PATH, or specify the full `path/to/zig` instead of `zig` in the commands below.

Clone the repository and run the following commands from its root directory:

```sh
zig build -Doptimize=ReleaseFast
zig build check
zig build test -Doptimize=ReleaseFast
```

For cross-compiling on Linux:

```sh
zig build -Dtarget=x86_64-windows-gnu -Doptimize=ReleaseFast -p zig-out-win
zig build test -Dtarget=x86_64-windows-gnu -Doptimize=ReleaseFast -p zig-out-win
```

## Using the Toolchain

```ats
#include "share/atspre_staload.hats"
implement main0 () = println!("Hello from ATS2!")
```

Save the example as `hello.dats`, then build and run it as follows:

On Linux:

```sh
/path/to/zig-out/bin/patscc -o hello hello.dats
./hello
```

On Windows:

```bat
C:\path\to\zig-out\bin\patscc.exe -o hello.exe hello.dats
hello.exe
```

## QoL

Drop-in usage on Linux:

```sh
unset PATSHOME PATSHOMERELOC
export PATH="full/path/to/ATS2-zigtools/zig-out/bin:$PATH"
```

```sh
which patscc
which patsopt

patsopt --version
patscc --version
```

The compiler automatically locates `PATSHOME` and `PATSHOMERELOC` relative to its own location.

### License
Licensed under the GNU General Public License, version 3 or (at your option).
Third-party components retain their original licenses.
