"""Amalgamate Arbiter library sources into a single source and header file.

Works with python2.6+ and python3.4+.

A near carbon copy of:
    https://github.com/open-source-parsers/jsoncpp/blob/master/amalgamate.py

Example of invocation (must be invoked from top directory):
python amalgamate.py
"""
import os
import os.path
import sys

class AmalgamationFile:
    def __init__(self, top_dir):
        self.top_dir = top_dir
        self.blocks = []

    def add_text(self, text):
        if not text.endswith("\n"):
            text += "\n"
        self.blocks.append(text)

    def add_file(self, relative_input_path, wrap_in_comment=False):
        def add_marker(prefix):
            self.add_text("")
            self.add_text("// " + "/"*70)
            self.add_text("// %s of content of file: %s" % (prefix, relative_input_path.replace("\\","/")))
            self.add_text("// " + "/"*70)
            self.add_text("")
        add_marker("Beginning")
        f = open(os.path.join(self.top_dir, relative_input_path), "rt")
        content = f.read()
        if wrap_in_comment:
            content = "/*\n" + content + "\n*/"
        self.add_text(content)
        f.close()
        add_marker("End")
        self.add_text("\n\n\n\n")

    def get_value(self):
        return "".join(self.blocks).replace("\r\n","\n")

    def write_to(self, output_path):
        output_dir = os.path.dirname(output_path)
        if output_dir and not os.path.isdir(output_dir):
            os.makedirs(output_dir)
        f = open(output_path, "wb")
        f.write(str.encode(self.get_value(), 'UTF-8'))
        f.close()

def amalgamate_source(source_top_dir=None,
                       target_source_path=None,
                       header_include_path=None):
    """Produces amalgamated source.
       Parameters:
           source_top_dir: top-directory
           target_source_path: output .cpp path
           header_include_path: generated header path relative to target_source_path.
    """

    gitsha = None

    try:
        f = open(".git/refs/heads/master")
        gitsha = f.read().rstrip()
        f.close()
    except:
        print("This script must be run from the top level")

    print("Amalgamating header...")
    header = AmalgamationFile(source_top_dir)
    header.add_text("/// Arbiter amalgamated header (https://github.com/connormanning/arbiter).")
    header.add_text('/// It is intended to be used with #include "%s"' % header_include_path)
    header.add_text('\n// Git SHA: ' + gitsha)
    header.add_file("LICENSE", wrap_in_comment=True)
    header.add_text("#pragma once")
    header.add_text("/// If defined, indicates that the source file is amalgamated")
    header.add_text("/// to prevent private header inclusion.")
    header.add_text("#define ARBITER_IS_AMALGAMATION")
    header.add_file("arbiter/driver.hpp")
    header.add_file("arbiter/drivers/fs.hpp")
    header.add_file("arbiter/drivers/http.hpp")
    header.add_file("arbiter/third/xml/rapidxml.hpp")
    header.add_file("arbiter/third/xml/xml.hpp")
    header.add_file("arbiter/util/crypto.hpp")
    header.add_file("arbiter/drivers/s3.hpp")
    header.add_file("arbiter/endpoint.hpp")
    header.add_file("arbiter/arbiter.hpp")

    target_header_path = os.path.join(os.path.dirname(target_source_path), header_include_path)
    print("Writing amalgamated header to %r" % target_header_path)
    header.write_to(target_header_path)

    print("Amalgamating source...")
    source = AmalgamationFile(source_top_dir)
    source.add_text("/// Arbiter amalgamated source (https://github.com/connormanning/arbiter).")
    source.add_text('/// It is intended to be used with #include "%s"' % header_include_path)
    source.add_file("LICENSE", wrap_in_comment=True)
    source.add_text("")
    source.add_text('#include "%s"' % header_include_path)
    source.add_text("""
#ifndef ARBITER_IS_AMALGAMATION
#error "Compile with -I PATH_TO_ARBITER_DIRECTORY"
#endif
""")
    source.add_text("")
    source.add_file("arbiter/arbiter.cpp")
    source.add_file("arbiter/driver.cpp")
    source.add_file("arbiter/endpoint.cpp")
    source.add_file("arbiter/drivers/fs.cpp")
    source.add_file("arbiter/drivers/http.cpp")
    source.add_file("arbiter/drivers/s3.cpp")
    source.add_file("arbiter/util/crypto.cpp")

    print("Writing amalgamated source to %r" % target_source_path)
    source.write_to(target_source_path)

def main():
    usage = """%prog [options]
Generate a single amalgamated source and header file from the sources.
"""
    from optparse import OptionParser
    parser = OptionParser(usage=usage)
    parser.allow_interspersed_args = False
    parser.add_option("-s", "--source", dest="target_source_path", action="store", default="dist/arbiter.cpp",
        help="""Output .cpp source path. [Default: %default]""")
    parser.add_option("-i", "--include", dest="header_include_path", action="store", default="arbiter.hpp",
        help="""Header include path. Used to include the header from the amalgamated source file. [Default: %default]""")
    parser.add_option("-t", "--top-dir", dest="top_dir", action="store", default=os.getcwd(),
        help="""Source top-directory. [Default: %default]""")
    parser.enable_interspersed_args()
    options, args = parser.parse_args()

    msg = amalgamate_source(source_top_dir=options.top_dir,
                             target_source_path=options.target_source_path,
                             header_include_path=options.header_include_path)
    if msg:
        sys.stderr.write(msg + "\n")
        sys.exit(1)
    else:
        print("Source successfully amalgamated")

if __name__ == "__main__":
    main()

