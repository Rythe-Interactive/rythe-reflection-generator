#include <rsl/cli>
#include <rsl/logging>
#include <rsl/set>
#include <rsl/string>
#include <rsl/threading>
#include <rsl/filesystem>

#include <clang-c/Index.h>

static CXTranslationUnit load_translation_unit(CXIndex index, const rfs::view& file);

int main(int argc, char* argv[])
{
    rsl::current_thread::set_name("Main thread");

    rsl::get_logging_context().logger = rsl::get_logging_context().undecoratedLogger;

    rsl::cli_parser cli;
    cli.set_command_usage("rrg [options] [<file> ...]\n\noptions:");
    cli.add_param({ "help", "h" }, true, "  --{:<21}\tDisplay help.");
    cli.add_param("version", true, "  --{:<21}\tDisplay version.");
    cli.add_param({ "verbose", "v" }, true, "  --{:<21}\tUse verbose logging.");
    cli.add_param("files", false, "  --{}=<filename>\t\tA file containing a list of files to process, one per line.");
    cli.add_param("pch", false, "  --{}=<filename>\t\tUse a precompiled header to speed up process.");
    cli.add_param("intermediates", false, "  --{}=<path>\tUse an intermediates folder to allow partial recompilation of changed files only.");
    cli.add_param("i", false, "  --{}=<path>\t\t\tUse an intermediates folder to allow partial recompilation of changed files only.");

    cli.parse(argc, argv);

    if (cli.has_flag({ "help", "h" }) || cli.is_empty())
    {
        cli.print_usage();
        return 0;
    }

    if (cli.has_flag("version"))
    {
        rlog::undecorated_info("rythe-reflection-generator v{}", 0);
        return 0;
    }

    cli.print_current_command();

    const bool verbose = cli.has_flag({ "verbose", "v" });

    if (verbose)
    {
        rlog::filter(rlog::severity::trace);
    }

    // lazy deduplicate
    const rsl::dynamic_array<rsl::string_view> files =
            rsl::dynamic_array<rsl::string_view>::from_view(rsl::dynamic_set<rsl::string_view>::from_view(cli.pos_args()).view());

    // lazy deduplicate
    const rsl::dynamic_array<rsl::string_view> pchFiles =
            rsl::dynamic_array<rsl::string_view>::from_view(rsl::dynamic_set<rsl::string_view>::from_view(cli.get_params("pch")).view());

    const rfs::view intermediatesPath = rfs::view(cli.get_param({ "intermediates", "i" })) / "rrg";
    const bool hasIntermediatesPath = intermediatesPath.is_valid(true);

    CXIndex index = clang_createIndex(!pchFiles.is_empty(), verbose);

    int translationUnitFlags = CXTranslationUnit_Incomplete | CXTranslationUnit_SkipFunctionBodies | CXTranslationUnit_KeepGoing |
            CXTranslationUnit_SingleFileParse | CXTranslationUnit_IncludeAttributedTypes |
            CXTranslationUnit_IgnoreNonErrorsFromIncludedFiles;

    if (pchFiles.is_empty())
    {
        translationUnitFlags |= CXTranslationUnit_PrecompiledPreamble | CXTranslationUnit_CreatePreambleOnFirstParse;
    }

    rlog::info("Loading pch:");
    {
        rlog::indent_scope indentScope{};

        for (auto& pch : pchFiles)
        {
            rlog::trace("{}", pch);
            CXTranslationUnit translationUnit;
            const CXErrorCode error = clang_createTranslationUnit2(index, pch.data(), &translationUnit);
            if (error != CXError_Success)
            {
                rsl::log::error("Failed to parse pch file \"{}\", pch will be ignored.", pch);
            }
            clang_disposeTranslationUnit(translationUnit);
        }
    }

    rlog::info("Processing files:");
    {
        rlog::indent_scope indentScope{};
        for (auto& file : files)
        {
            rlog::trace("{}", file);
            rlog::indent_scope fileIndentScope{};

            rfs::view intermediateFile;
            CXTranslationUnit translationUnit = nullptr;
            if (hasIntermediatesPath)
            {
                rlog::trace("Computing content hash.");
                rsl::result<rsl::byte_view> data = rfs::view(file).read();
                if (data.has_errors())
                {
                    return rsl::narrowing_cast<int>(data.report_errors_and_resolve());
                }

                const rsl::content_hash content = rsl::hash_content(data.value());
                intermediateFile = intermediatesPath /
                        rsl::format("{}/{}/{}/{}/{}.rrg_ast",
                                    content.value.u32[0],
                                    content.value.u32[1],
                                    content.value.u32[2],
                                    content.value.u32[3],
                                    content.size);

                rlog::trace("Loading intermediate file \"{}\".", intermediateFile.path());
                translationUnit = load_translation_unit(index, intermediateFile);
            }

            if (!translationUnit)
            {
                rlog::trace("Parsing translation unit.");
                const CXErrorCode error = clang_parseTranslationUnit2(
                        index, file.data(), nullptr, 0, nullptr, 0, translationUnitFlags, &translationUnit);
                if (error != CXError_Success)
                {
                    rsl::log::error("Failed to parse source file \"{}\".", file);
                    return error;
                }
            }

            if (hasIntermediatesPath)
            {
                rlog::trace("Saving translation unit to intermediate file.");
                const int error =
                        clang_saveTranslationUnit(translationUnit, intermediatesPath.path().data(), CXSaveTranslationUnit_None);
                if (error != CXSaveError_None)
                {
                    rsl::log::error("Failed to save intermediate file \"{}\" for source file \"{}\".", intermediatesPath.path(), file);
                }
            }

            // do something

            clang_disposeTranslationUnit(translationUnit);
        }
    }

    return 0;
}

static CXTranslationUnit load_translation_unit(CXIndex index, const rfs::view& file)
{
    if (!file.exists())
    {
        return nullptr;
    }

    CXTranslationUnit translationUnit;
    CXErrorCode error = clang_createTranslationUnit2(index, file.path().data(), &translationUnit);
    if (error != CXError_Success)
    {
        rsl::log::error("Failed to load intermediate file \"{}\", will attempt to parse from source.", file.path());
        return nullptr;
    }

    return translationUnit;
}
