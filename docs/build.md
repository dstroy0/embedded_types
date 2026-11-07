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
python test/harness.py suites --strict          # each case registered, none behind a conditional
```

The configure step fetches [Unity](https://github.com/ThrowTheSwitch/Unity) at `v2.6.1`. The runner generator of each suite needs `ruby` on `PATH`. Both belong to `test/`, and CMake configures `test/` only where `EMBEDDED_TYPES_BUILD_TESTS` is on. That option is on by default where this project is the top-level project.

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
