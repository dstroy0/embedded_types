# Build and use

The library is headers only, so a build compiles nothing until a consumer includes a header. The suite builds and runs one Unity program for each header.

## Build and test

1. Configure a build tree, with each warning an error:

    ```sh
    cmake -S . -B build -DEMBEDDED_TYPES_WERROR=ON
    ```

2. Build the suites:

    ```sh
    cmake --build build
    ```

3. Run the suites:

    ```sh
    ctest --test-dir build --output-on-failure
    ```

`test/harness.py` runs the same builds, with the flags of each tree:

```sh
python test/harness.py test                     # the library as it ships
python test/harness.py test --tree build-werror # the same, with each warning an error
python test/harness.py test --tree build-tsan   # the same, under the thread sanitizer
python test/harness.py test --tree build-asan   # the same, under the address and undefined behavior sanitizers
python test/harness.py coverage --gaps          # the coverage of include/, and each line and branch that no suite reached
python test/harness.py suites --strict          # each case registered, none behind a conditional
python test/harness.py prove                    # each CBMC harness, and each thread client under each memory model
python test/harness.py ste                      # each document held to ASD-STE100
```

The configure step fetches [Unity](https://github.com/ThrowTheSwitch/Unity) at `v2.6.1`. The runner generator of each suite needs `ruby` on `PATH`. Both belong to `test/`, and CMake configures `test/` only where `EMBEDDED_TYPES_BUILD_TESTS` is on. That option is on by default where this project is the top-level project.

Nine suites run, one for each header and one for the device arm of the lane. CTest also runs the ASD-STE100 check as a test of its own. Where the configure step finds a CUDA compiler, the device suite compiles its kernels and runs them on the part that the host has. Where it finds none, the build leaves that suite out and says so.

## Coverage

`coverage` builds the tree `build-cov` with GCC and `--coverage`, runs the suites, and reads the counters with gcovr. It writes `build-cov/coverage.xml`, the report that the SonarCloud scan reads. With `--gaps`, it names each line and each branch of `include/` that no suite reached, and a gap fails the run.

1. Install gcovr from its lock:

    ```sh
    python -m pip install --require-hashes --only-binary :all: -r requirements-coverage.txt
    ```

2. Run the coverage:

    ```sh
    python test/harness.py coverage --gaps
    ```

On Windows, run both steps under WSL, and set `EMBEDDED_TYPES_BUILD_ROOT` to a folder of its own. The trees of the Windows build then stay apart from the trees of WSL.

The coverage build includes `test/support/coverage_inline.h` ahead of each unit. That file defines `EMBED_INLINE` as a plain `static inline`. So each header function has one body in each unit, and the report counts the branches of the source.

The SonarCloud workflow runs the same command under the build wrapper of SonarCloud, then the scan. The scan needs the `SONAR_TOKEN` secret, and the automatic analysis of SonarCloud off for this project.

## Use in a CMake build

```cmake
add_subdirectory(embedded_types)
target_link_libraries(your_target PRIVATE embedded_types::embedded_types)
```

`embedded_types::embedded_types` is an INTERFACE target. It gives the include directory and `c_std_11`, and no compile options. The warning settings of the consumer apply to each translation unit that includes these headers.

PlatformIO reads `library.json`, and the Arduino IDE reads `library.properties`.

## Build this site

1. Install the site tools:

    ```sh
    python -m pip install -r requirements-docs.txt
    ```

2. After an edit to a header, write the API pages again:

    ```sh
    python tools/api_pages.py
    ```

3. Build the site into `build/site`:

    ```sh
    python test/harness.py site
    ```

`tools/api_pages.py` makes one API page in `docs/api` for each header, from its Doxygen comments. Commit each page with its header. The site build and the pre-commit hook stop where a page does not match its header. They also stop at each line of a page that breaks an ASD-STE100 rule, and they name the header line.
