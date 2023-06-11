# Contributing Guidelines

1. [ IDE. ](#ide)
2. [ Coding style ](#code_style)
3. [Debugging & Analysis](#debug)

<a name="ide"></a>
## IDE

### JuCi++ (recommended)

https://gitlab.com/cppit/jucipp

Preferences:

-   "cleanup_whitespace_characters": "true"
-   "show_whitespace_characters": "space, tab, leading, nbsp, trailing"
-   "highlight_current_line": "true"
-   "right_margin_position": "80"
-   "auto_tab_char_and_size": "true"
-   "default_tab_char_comment": "Use \"\t\" for regular tab"
-   "default_tab_char": "\t"
-   "default_tab_size": "1"
-   "tab_indents_line": "true"

### Atom Editor

If you're using Atom Editor you may use the following settings and packages:

#### Settings

File » Settings » Editor »

-   Show Invisibles: on
-   Show Line Numbers: on
-   Soft Tabs: on
-   Tab Length: 4
-   Tab Type: hard

#### Packages

File » Settings » Install »

-   atom-discord
-   linter
-   markdown-preview
-   platformio-ide-terminal (may have problems recognizing environment variables)
-   atom-beautify
-   todo-show

<a name="debug"></a>
## Debugging & Analysis

- [Cppcheck](http://cppcheck.sourceforge.net/) is a static analysis tool for C/C++ code.
  Check with `cppcheck --enable=all source/`
- [Clang Analyser](https://wiki.archlinux.org/index.php/clang#Using_the_Static_Analyzer) - Use like `scan-build make debug`
- [clang-tidy](http://www.labri.fr/perso/fleury/posts/programming/using-clang-tidy-and-clang-format.html)
  Use like `clang-tidy source/*.cpp -checks=*,-clang-analyzer-alpha.*,-llvm-include-order -- -std=c++14 -Iinclude > out.txt`
- Valgrind to check memory leaks.
  Note: Valgrind doesn't work with sanitizers. Info [here](https://stackoverflow.com/questions/42079091/valgrind-gcc-6-2-0-and-fsanitize-address).

More tips: https://lefticus.gitbooks.io/cpp-best-practices/content/02-Use_the_Tools_Available.html

### Code coverage

Ubuntu: 
```sh
sudo apt install gcovr ggcov lcov
```

Arch Linux:
```sh
sudo pacman -Sy gcovr
# Install lcov from AUR
# https://aur.archlinux.org/packages/lcov-git/
```

```sh
# Run tests first
make debug -j8
./bin/debug/libswf_test

gcov source/dynamic_bitset.cpp --object-directory build/

lcov --base-directory . --directory build/ --capture --output-file coverage.info

lcov --list coverage.info

genhtml coverage.info -o coverage
```

```
I'm using coveralls + travis CI and am liking it. It gives you nice coverage report for each build and pull request and coverage history for the whole project and each source file. To use it:

Project is built in travis as usual then tests are run, just --coverage is added to compiler and linker flags. For CMake, I setup a special build type:

  SET(CMAKE_CXX_FLAGS_COVERAGE "${CMAKE_CXX_FLAGS_DEBUG} --coverage")
  SET(CMAKE_EXE_LINKER_FLAGS_COVERAGE "${CMAKE_EXE_LINKER_FLAGS_DEBUG} --coverage")
  SET(CMAKE_SHARED_LINKER_FLAGS_COVERAGE "${CMAKE_SHARED_LINKER_FLAGS_DEBUG} --coverage")
and build with -DCMAKE_BUILD_TYPE=Coverage

A couple of lines are added to .travis.yml to upload coverage info to coveralls:

Before build: pip install --user pyyaml cpp-coveralls

After successful build: coveralls or coveralls -i <dir> if you need to limit coverage with specific directory

Sign in to coveralls and add your repo. With next build you'll get coverage info.
```



<a name="code_style"></a>

## Coding Style

Use tabs instead of spaces (unless for precision). Terminate lines with newline character.

Namespace names should be `snake_case`.
Class names should be upper `CamelCase`.
Function, method and variable names should be lower `camelCase`.

<a name="translating"></a>
