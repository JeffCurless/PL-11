# CMake generated Testfile for 
# Source directory: /home/curless/workspace/pl11
# Build directory: /home/curless/workspace/pl11/build-llvm
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test(lex_hello "/home/curless/workspace/pl11/build-llvm/pl11c" "--lex" "/home/curless/workspace/pl11/tests/hello.pl11")
set_tests_properties(lex_hello PROPERTIES  _BACKTRACE_TRIPLES "/home/curless/workspace/pl11/CMakeLists.txt;98;add_test;/home/curless/workspace/pl11/CMakeLists.txt;0;")
add_test(parse_hello "/home/curless/workspace/pl11/build-llvm/pl11c" "--parse" "/home/curless/workspace/pl11/tests/hello.pl11")
set_tests_properties(parse_hello PROPERTIES  _BACKTRACE_TRIPLES "/home/curless/workspace/pl11/CMakeLists.txt;103;add_test;/home/curless/workspace/pl11/CMakeLists.txt;0;")
add_test(sema_hello "/home/curless/workspace/pl11/build-llvm/pl11c" "--sema" "/home/curless/workspace/pl11/tests/hello.pl11")
set_tests_properties(sema_hello PROPERTIES  _BACKTRACE_TRIPLES "/home/curless/workspace/pl11/CMakeLists.txt;108;add_test;/home/curless/workspace/pl11/CMakeLists.txt;0;")
add_test(emit_llvm_hello "/home/curless/workspace/pl11/build-llvm/pl11c" "--emit-llvm" "/home/curless/workspace/pl11/tests/hello.pl11")
set_tests_properties(emit_llvm_hello PROPERTIES  _BACKTRACE_TRIPLES "/home/curless/workspace/pl11/CMakeLists.txt;114;add_test;/home/curless/workspace/pl11/CMakeLists.txt;0;")
