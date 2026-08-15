#include <rsl/cli>
#include <rsl/logging>
#include <rsl/set>
#include <rsl/string>
#include <rsl/threading>

#include <clang-c/Index.h>

int main(int argc, char* argv[])
{
    rsl::current_thread::set_name("Main thread");
    rsl::log::filter(rsl::log::severity::debug);

#if defined(RYTHE_DEBUG)
    rsl::log::flush_at(rsl::log::severity::debug);
#endif

    rsl::cli_parser cmdl;
    cmdl.set_command_usage("rrg [options] [<file> ...]\n\noptions:");
    cmdl.add_param({ "help", "h" }, true, "  --{:<21}\tDisplay help.");
    cmdl.add_param("version", true, "  --{:<21}\tDisplay version.");
    cmdl.add_param({ "verbose", "v" }, true, "  --{:<21}\tUse verbose logging.");
    cmdl.add_param("files", false, "  --{}=<filename>\t\tA file containing a list of files to process, one per line.");
    cmdl.add_param("pch", false, "  --{}=<filename>\t\tUse a precompiled header to speed up process.");
    cmdl.add_param("intermediates", false, "  --{}=<path>\tUse an intermediates folder to allow partial recompilation of changed files only.");
    cmdl.add_param("i", false, "  --{}=<path>\t\t\tUse an intermediates folder to allow partial recompilation of changed files only.");

    cmdl.parse(argc, argv);

    if (cmdl.has_flag({ "help", "h" }) || cmdl.is_empty() || true)
    {
        cmdl.print_usage();
        return 0;
    }

    if (cmdl.has_flag("version"))
    {
        rsl::log::undecorated_info("rythe-reflection-generator v{}", 0);
        return 0;
    }

    const bool verbose = cmdl.has_flag({ "verbose", "v" });

    if (verbose)
    {
        rsl::log::filter(rsl::log::severity::trace);
    }

    // lazy deduplicate
    rsl::dynamic_array<rsl::string_view> files =
            rsl::dynamic_array<rsl::string_view>::from_view(rsl::dynamic_set<rsl::string_view>::from_view(cmdl.pos_args()).view());

    // lazy deduplicate
    rsl::dynamic_array<rsl::string_view> pchFiles =
            rsl::dynamic_array<rsl::string_view>::from_view(rsl::dynamic_set<rsl::string_view>::from_view(cmdl.get_params("pch")).view());

    CXIndex index = clang_createIndex(!pchFiles.is_empty(), verbose);

    int translationUnitFlags = CXTranslationUnit_Incomplete | CXTranslationUnit_SkipFunctionBodies | CXTranslationUnit_KeepGoing |
            CXTranslationUnit_SingleFileParse | CXTranslationUnit_IncludeAttributedTypes |
            CXTranslationUnit_IgnoreNonErrorsFromIncludedFiles;

    if (pchFiles.is_empty())
    {
        translationUnitFlags |= CXTranslationUnit_PrecompiledPreamble | CXTranslationUnit_CreatePreambleOnFirstParse;
    }

    for (auto& pch : pchFiles)
    {
        CXTranslationUnit translationUnit;
        CXErrorCode error = clang_createTranslationUnit2(index, pch.data(), &translationUnit);
        if (error != CXError_Success)
        {
            rsl::log::error("failed to parse pch file \"{}\", pch will be ignored", pch);
        }
        clang_disposeTranslationUnit(translationUnit);
    }

    for (auto& file : files)
    {
        CXTranslationUnit translationUnit;
        CXErrorCode error =
                clang_parseTranslationUnit2(index, file.data(), nullptr, 0, nullptr, 0, translationUnitFlags, &translationUnit);
        if (error != CXError_Success)
        {
            rsl::log::error("failed to parse source file: \"{}\"", file);
            return error;
        }
        clang_disposeTranslationUnit(translationUnit);
    }

    return 0;
}
