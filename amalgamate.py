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
                       header_include_path=None,
                       include_json=True,
                       include_xml=True,
                       custom_namespace=None,
                       define_curl=True):
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

    if include_json:
        print("Bundling JSON with amalgamation")
    else:
        print("NOT bundling JSON with amalgamation")

    if include_xml:
        print("Bundling RapidXML with amalgamation")
    else:
        print("NOT bundling RapidXML with amalgamation")


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

    if custom_namespace:
        print "Using custom namespace: " + custom_namespace
        header.add_text("#define ARBITER_CUSTOM_NAMESPACE " + custom_namespace)

    if not include_json:
        print "NOT bundling JSON"
        header.add_text("#define ARBITER_EXTERNAL_JSON")
    if not include_xml:
        print "NOT bundling XML"
        header.add_text("#define ARBITER_EXTERNAL_XML")

    if include_json:
        header.add_file("arbiter/third/json/json.hpp")

    if define_curl:
        header.add_text("\n#pragma once")
        header.add_text("#define ARBITER_CURL")
    else:
        print "NOT #defining ARBITER_CURL"

    header.add_file("arbiter/util/exports.hpp")
    header.add_file("arbiter/util/types.hpp")
    header.add_file("arbiter/util/curl.hpp")
    header.add_file("arbiter/util/http.hpp")
    header.add_file("arbiter/util/ini.hpp")
    header.add_file("arbiter/util/time.hpp")

    header.add_file("arbiter/driver.hpp")
    header.add_file("arbiter/drivers/fs.hpp")
    header.add_file("arbiter/drivers/http.hpp")

    if include_xml:
        header.add_file("arbiter/third/xml/rapidxml.hpp")
        header.add_file("arbiter/third/xml/xml.hpp")

    header.add_file("arbiter/util/macros.hpp")
    header.add_file("arbiter/util/md5.hpp")
    header.add_file("arbiter/util/sha256.hpp")
    header.add_file("arbiter/util/transforms.hpp")
    header.add_file("arbiter/util/util.hpp")
    header.add_file("arbiter/drivers/s3.hpp")
    header.add_file("arbiter/drivers/google.hpp")
    header.add_file("arbiter/drivers/dropbox.hpp")
    header.add_file("arbiter/drivers/test.hpp")
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

    if include_json:
        source.add_file("arbiter/third/json/jsoncpp.cpp")

    source.add_file("arbiter/drivers/fs.cpp")
    source.add_file("arbiter/drivers/http.cpp")
    source.add_file("arbiter/drivers/s3.cpp")
    source.add_file("arbiter/drivers/google.cpp")
    source.add_file("arbiter/drivers/dropbox.cpp")
    source.add_file("arbiter/util/curl.cpp")
    source.add_file("arbiter/util/http.cpp")
    source.add_file("arbiter/util/ini.cpp")
    source.add_file("arbiter/util/md5.cpp")
    source.add_file("arbiter/util/sha256.cpp")
    source.add_file("arbiter/util/transforms.cpp")
    source.add_file("arbiter/util/time.cpp")
    source.add_file("arbiter/util/util.cpp")

    print("Writing amalgamated source to %r" % target_source_path)
    source.write_to(target_source_path)

def main():
    usage = """%prog [options]
Generate a single amalgamated source and header file from the sources.
"""
    from optparse import OptionParser
    parser = OptionParser(usage=usage)
    parser.allow_interspersed_args = False

    parser.add_option(
            "-s", "--source",
            dest="target_source_path",
            action="store",
            default="dist/arbiter.cpp",
            help="""Output .cpp source path. [Default: %default]""")

    parser.add_option(
            "-i", "--include",
            dest="header_include_path",
            action="store",
            default="arbiter.hpp",
            help="""Header include path. Used to include the header from the amalgamated source file. [Default: %default]""")

    parser.add_option(
            "-t", "--top-dir",
            dest="top_dir",
            action="store",
            default=os.getcwd(),
            help="""Source top-directory. [Default: %default]""")

    parser.add_option(
            "-j", "--no-include-json",
            dest="include_json",
            action="store_false",
            default=True)

    parser.add_option(
            "-x", "--no-include-xml",
            dest="include_xml",
            action="store_false",
            default=True)

    parser.add_option(
            "-d", "--define-curl",
            dest="define_curl",
            action="store_true",
            default=False)

    parser.add_option(
            "-c", "--custom-namespace",
            dest="custom_namespace",
            action="store",
            default=None)

    parser.enable_interspersed_args()
    options, args = parser.parse_args()

    msg = amalgamate_source(source_top_dir=options.top_dir,
                             target_source_path=options.target_source_path,
                             header_include_path=options.header_include_path,
                             include_json=options.include_json,
                             include_xml=options.include_xml,
                             custom_namespace=options.custom_namespace,
                             define_curl=options.define_curl)
    if msg:
        sys.stderr.write(msg + "\n")
        sys.exit(1)
    else:
        print("Source successfully amalgamated")

if __name__ == "__main__":
    main()

