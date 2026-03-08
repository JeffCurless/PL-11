#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <cstring>

#include "lexer/pl11_lexer.h"
#include "parser/pl11_parser.h"
#include "sema/pl11_sema.h"

// LLVM codegen is conditionally compiled
#ifdef PL11_WITH_LLVM
#include "codegen/pl11_codegen.h"
#include <llvm/IR/LLVMContext.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Target/TargetMachine.h>
#include <llvm/Target/TargetOptions.h>
#include <llvm/MC/TargetRegistry.h>
#include <llvm/TargetParser/Host.h>
#endif

static void usage(const char* prog) {
    std::cerr << "Usage: " << prog << " [options] <input.pl11>\n"
              << "\n"
              << "Options:\n"
              << "  --lex           Print tokens and exit\n"
              << "  --parse         Parse and print AST summary, exit\n"
              << "  --sema          Semantic analysis only, exit\n"
              << "  --emit-llvm     Emit LLVM IR to stdout\n"
              << "  -o <file>       Output file (default: a.out)\n"
              << "  -h, --help      Show this help\n";
}

static std::string readFile(const std::string& path) {
    std::ifstream f(path);
    if (!f.is_open()) {
        throw std::runtime_error("Cannot open file: " + path);
    }
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

int main(int argc, char* argv[]) {
    if (argc < 2) { usage(argv[0]); return 1; }

    std::string inputFile;
    std::string outputFile = "a.out";
    bool doLex     = false;
    bool doParse   = false;
    bool doSema    = false;
    bool emitLLVM  = false;

    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--lex") == 0)      doLex    = true;
        else if (std::strcmp(argv[i], "--parse") == 0)  doParse  = true;
        else if (std::strcmp(argv[i], "--sema") == 0)   doSema   = true;
        else if (std::strcmp(argv[i], "--emit-llvm") == 0) emitLLVM = true;
        else if (std::strcmp(argv[i], "-o") == 0 && i + 1 < argc) outputFile = argv[++i];
        else if (std::strcmp(argv[i], "-h") == 0 || std::strcmp(argv[i], "--help") == 0) {
            usage(argv[0]); return 0;
        } else if (argv[i][0] != '-') {
            inputFile = argv[i];
        } else {
            std::cerr << "Unknown option: " << argv[i] << "\n";
            usage(argv[0]);
            return 1;
        }
    }

    if (inputFile.empty()) {
        std::cerr << "Error: No input file specified.\n";
        usage(argv[0]);
        return 1;
    }

    std::string source;
    try {
        source = readFile(inputFile);
    } catch (const std::exception& e) {
        std::cerr << e.what() << "\n";
        return 1;
    }

    // ---- Lexing ----
    pl11::Lexer lexer(source, inputFile);

    if (doLex) {
        auto tokens = lexer.tokenize();
        for (const auto& tok : tokens) {
            std::cout << tok.loc.line << ":" << tok.loc.col
                      << "\t" << pl11::tokenKindName(tok.kind)
                      << "\t'" << tok.text << "'\n";
        }
        return 0;
    }

    // ---- Parsing ----
    std::unique_ptr<pl11::ProgramNode> program;
    try {
        pl11::Parser parser(lexer);
        program = parser.parseProgram();
    } catch (const pl11::ParseError& e) {
        std::cerr << e.what() << "\n";
        return 1;
    }

    if (doParse) {
        std::cout << "Parse successful. Program block has "
                  << program->block->decls.size() << " declaration(s) and "
                  << program->block->stmts.size() << " statement(s).\n";
        return 0;
    }

    // ---- Semantic analysis ----
    try {
        pl11::Sema sema;
        sema.analyse(*program);
    } catch (const pl11::SemanticError& e) {
        std::cerr << e.what() << "\n";
        return 1;
    }

    if (doSema) {
        std::cout << "Semantic analysis passed.\n";
        return 0;
    }

    // ---- Code generation ----
#ifdef PL11_WITH_LLVM
    llvm::InitializeAllTargetInfos();
    llvm::InitializeAllTargets();
    llvm::InitializeAllTargetMCs();
    llvm::InitializeAllAsmParsers();
    llvm::InitializeAllAsmPrinters();

    llvm::LLVMContext ctx;
    try {
        pl11::Codegen codegen(ctx, inputFile);
        codegen.generate(*program);

        if (emitLLVM) {
            codegen.emitIR();
            return 0;
        }

        // Write to object file
        std::string targetTriple = llvm::sys::getDefaultTargetTriple();
        std::string err;
        const llvm::Target* target = llvm::TargetRegistry::lookupTarget(targetTriple, err);
        if (!target) {
            std::cerr << "Target lookup failed: " << err << "\n";
            return 1;
        }
        llvm::TargetOptions opt;
        auto* tm = target->createTargetMachine(targetTriple, "generic", "", opt,
                                                std::optional<llvm::Reloc::Model>());
        codegen.getModule()->setDataLayout(tm->createDataLayout());
        codegen.getModule()->setTargetTriple(targetTriple);

        std::error_code ec;
        llvm::raw_fd_ostream dest(outputFile + ".o", ec, llvm::sys::fs::OF_None);
        if (ec) {
            std::cerr << "Cannot open output: " << ec.message() << "\n";
            return 1;
        }
        llvm::legacy::PassManager pm;
        if (tm->addPassesToEmitFile(pm, dest, nullptr,
                                     llvm::CodeGenFileType::ObjectFile)) {
            std::cerr << "Cannot emit object file\n";
            return 1;
        }
        pm.run(*codegen.getModule());
        dest.flush();
        std::cout << "Object code written to " << outputFile << ".o\n";
    } catch (const pl11::CodegenError& e) {
        std::cerr << "Codegen error: " << e.what() << "\n";
        return 1;
    }
#else
    if (emitLLVM) {
        std::cerr << "Error: pl11c was built without LLVM support (rebuild with -DPL11_WITH_LLVM=ON)\n";
        return 1;
    }
    std::cout << "Compilation successful (parse + sema only; link with LLVM for code generation).\n";
#endif

    return 0;
}
