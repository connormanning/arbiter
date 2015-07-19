# Arbiter

Arbiter provides simple and thread-safe C++ access to filesystem, HTTP, and S3 resources in a uniform way.

## Using Arbiter in your project

### Installation

Arbiter uses CMake for its build process.  To build and install, run:

```bash
git clone git@github.com:connormanning/arbiter.git
cd arbiter && mkdir build && cd build
cmake -G "<CMake generator type>" ..    # For example: cmake -G "Unix Makefiles" ..
make
make install
```

Then simply include the header in your project:

```cpp
#include <arbiter/arbiter.h>
```

### Amalgamation

The amalgamation method lets you integrate Arbiter into your project by compiling a single source and including a single header file.  Create the amalgamation by running from the top level:

`python amalgamate.py`

Then copy `dist/arbiter.hpp` and `dist/arbiter.cpp` into your project tree and include them in your build system like any other source files.  With this method you'll need to link the [Curl dependency](#dependencies) into your project manually.

### Dependencies

Arbiter depends on [Curl](http://curl.haxx.se/libcurl/), which comes preinstalled on most UNIX-based machines.  To manually link (for amalgamated usage) on Unix-based operating systems, link with `-lcurl`.

## API sample

```cpp
using namespace arbiter;

// Arbiter a;                               // Filesystem and HTTP access.
Arbiter a(AwsAuth("public", "private"));    // Supply auth for S3 access.

std::string fsPath, httpPath, s3Path;
std::string fsData, httpData, s3Data;
std::vector<std::string> fsGlob, s3Glob;

// Read and write data.
fsPath = "~/fs.txt";
a.put(fsPath, "Filesystem contents!");
fsData = a.get(fsPath);

httpPath = "http://some-server.com/http.txt";
a.put(httpPath, "HTTP contents!");
httpData = a.get(httpPath);

s3Path = "s3://some-bucket/s3.txt";
a.put(s3Path, "S3 contents!");
s3Data = a.get(s3Path);

// Resolve globbed paths.
fsGlob = a.resolve("~/data/*");
s3Glob = a.resolve("s3://some-bucket/some-dir/*");
```

