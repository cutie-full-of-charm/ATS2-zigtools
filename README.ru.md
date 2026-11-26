# ATS2-zigtools

**Английский:** [README.md](README.md)

Порт [Applied Type System 2](https://ats-lang.sourceforge.net/) (Postiats 0.4.2) компилятора и стандартной библиотеки.
Для того, чтобы сбилдить компилятор и стандартную библиотеку нужен только `Zig ver. 0.17.0` и команда `zig build`.  
Стандартная библиотека пропатчена для нативной работы на windows, не имея зависимости от cygwin, в отличие от оригинальной.  

## Билд и тестирование
Требования: `Zig 0.17.0` доступный в PATH, или указывайте полный `path/to/zig` вместо `zig` в приведенных ниже командах:  

Склонируйте репо, запускайте из корня его директории
```sh
zig build -Doptimize=ReleaseFast
zig build check
zig build test -Doptimize=ReleaseFast
```

Для кросскомпиляции на линукс:

```sh
zig build -Dtarget=x86_64-windows-gnu -Doptimize=ReleaseFast -p zig-out-win
zig build test -Dtarget=x86_64-windows-gnu -Doptimize=ReleaseFast -p zig-out-win
```

## Использование тулчейна:

```ats
#include "share/atspre_staload.hats"
implement main0 () = println!("Hello from ATS2!")
```

Сохраните пример как `hello.dats`, далее можно его сбилдить:

На linux:
```sh
/path/to/zig-out/bin/patscc -o hello hello.dats
./hello
```

На windows:

```bat
C:\path\to\zig-out\bin\patscc.exe -o hello.exe hello.dats
hello.exe
```

## QoL

drop-in использование на Linux:
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
\
Компилятор сам найдет `PATSHOME` и `PATSHOMERELOC` относительно себя.

### License
Лицензировано на условиях GNU General Public License, версия 3 или (на ваш выбор) более поздняя. 
На сторонние компоненты продолжают действовать их первоначальные лицензии.
