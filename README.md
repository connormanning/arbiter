# Arbiter

Arbiter provides simple/fast/thread-safe C++ access to filesystem, HTTP, and S3 resources in a uniform way.

## API sample

The core API is intended to be as simple as possible.

```cpp
using namespace arbiter;

// Arbiter a;                               // Filesystem and HTTP access.
Arbiter a(AwsAuth("public", "private"));    // Supply auth for S3 access.

std::string fsPath, httpPath, s3Path;
std::string fsData, httpData, s3Data;
std::vector<std::string> fsGlob, s3Glob;

// Read and write data.
fsPath = "~/fs.txt";  // Tilde expansion is supported on Unix.
a.put(fsPath, "Filesystem contents!");
fsData = a.get(fsPath);

httpPath = "http://some-server.com/http.txt";
a.put(httpPath, "HTTP contents!");
httpData = a.get(httpPath);

s3Path = "s3://some-bucket/s3.txt";
a.put(s3Path, "S3 contents!");
s3Data = a.get(s3Path);

// Resolve globbed directory paths.
fsGlob = a.resolve("~/data/*");
s3Glob = a.resolve("s3://some-bucket/some-dir/*");
```

## Using Arbiter in your project

### Installation

Arbiter uses CMake for its build process.  To build and install, run from the top level:

```bash
mkdir build && cd build
cmake -G "<CMake generator type>" ..    # For example: cmake -G "Unix Makefiles" ..
make
make install
```

Then simply include the header in your project:

```cpp
#include <arbiter/arbiter.h>
```

...and link with the library with `-larbiter`.

### Amalgamation

The amalgamation method lets you integrate Arbiter into your project by adding a single source and a single header to your project.  Create the amalgamation by running from the top level:

`python amalgamate.py`

Then copy `dist/arbiter.hpp` and `dist/arbiter.cpp` into your project tree and include them in your build system like any other source files.  With this method you'll need to link the Curl [dependency](#dependencies) into your project manually.

Once the amalgamated files are integrated with your source tree, simply `#include "arbiter.hpp"` and get to work.

### Dependencies

Arbiter depends on [Curl](http://curl.haxx.se/libcurl/), which comes preinstalled on most UNIX-based machines.  To manually link (for amalgamated usage) on Unix-based operating systems, link with `-lcurl`.  Arbiter also works on Windows, but you'll have to obtain Curl yourself there.

Arbiter requires C++11.
