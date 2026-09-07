#include <rsl/cli>
#include <rsl/filesystem>
#include <rsl/logging>
#include <rsl/set>
#include <rsl/string>
#include <rsl/threading>
#include <rsl/time>
#include <rsl/utilities>

#include <clang-c/Index.h>

#include "reflection_generator.hpp"

static CXTranslationUnit load_translation_unit(CXIndex index, const rfs::view& file);
static void save_translation_unit(CXTranslationUnit translationUnit, const rfs::view& file);

enum struct output_mode
{
    invalid,
    individual,
    grouped,
};

int main(int argc, char* argv[])
{
    rsl::current_thread::set_name("Main thread");

    rsl::get_logging_context().logger = rsl::get_logging_context().undecoratedLogger;
    rlog::set_indent_char('\t');

    rsl::cli_parser cli;
    cli.set_command_usage("rrg [options] [<file> ...]\n\noptions:");
    cli.add_param({ "help", "h" }, true, "  --{:<21}\tDisplay help.");
    cli.add_param("version", true, "  --{:<21}\tDisplay version.");
    cli.add_param({ "verbose", "v" }, true, "  --{:<21}\tUse verbose logging.");
    cli.add_param({ "clean", "c" }, true, "  --{:<21}\tKeep track of used intermediate files, and delete all unused ones at the end.");
    cli.add_param("pch", false, "  --{}=<filename>\t\tUse a precompiled header to speed up process.");

    cli.add_param(
            "intermediates",
            false,
            "  --{}=<path>\tUse an intermediates folder to allow partial recompilation of changed files only.");
    cli.add_param("i", false, "  --{}=<path>\t\t\tUse an intermediates folder to allow partial recompilation of changed files only.");

    cli.add_param("output-mode", false, "  --{}=<mode>\tSet the way --output should be interpreted. i=individual for each generated output relative to the source file, g=grouped output folder for all generated outputs. Default value: \"i\"");
    cli.add_param("om", false, "  --{}=<mode>\t\t\tSet the way --output should be interpreted. i=individual for each generated output relative to the source file, g=grouped output folder for all generated outputs. Default value: \"i\"");

    cli.add_param("output", false, "  --{}=<path>\t\tTarget output folder of all generated files. Default value: \"generated/\"");
    cli.add_param("o", false, "  --{}=<path>\t\t\tTarget output folder of all generated files. Default value: \"generated/\"");

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

    output_mode outputMode = output_mode::invalid;
    rsl::string_view outputModeParam = cli.get_param({ "output-mode", "om" }, "i");
    if (outputModeParam.size() == 1ull)
    {
        if (outputModeParam[0] == 'i')
        {
            outputMode = output_mode::individual;
        }
        else if (outputModeParam[0] == 'g')
        {
            outputMode = output_mode::grouped;
        }
    }

    if (outputMode == output_mode::invalid)
    {
        rlog::error("Output mode \"{}\" is invalid", outputModeParam);
        return -1;
    }

    const rsl::dynamic_string outputPath = rfs::standardize(cli.get_param({ "output", "o" }, "generated/"));

    // lazy deduplicate
    const rsl::dynamic_array<rsl::string_view> files =
            rsl::dynamic_array<rsl::string_view>::from_view(rsl::dynamic_set<rsl::string_view>::from_view(cli.pos_args()).view());

    // lazy deduplicate
    const rsl::dynamic_array<rsl::string_view> pchFiles = rsl::dynamic_array<rsl::string_view>::from_view(
            rsl::dynamic_set<rsl::string_view>::from_view(cli.get_params("pch")).view());

    const rfs::view intermediatesPath = rfs::view(cli.get_param({ "intermediates", "i" }), true) / "rrg/";
    bool hasIntermediatesPath = intermediatesPath.is_valid(true);

    if (!intermediatesPath.exists())
    {
        rlog::info("Creating intermediates folder.");
        rsl::result<void> creationResult = intermediatesPath.create();
        if (creationResult.has_errors())
        {
            rlog::indent_scope indentScope{};
            for (const rsl::error_type& error : creationResult.get_errors())
            {
                rlog::trace("{}: {}", error.code, error.message);
            }

            creationResult.resolve();
            hasIntermediatesPath = false;
        }
    }

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

        rsl::pointer<rsl::dynamic_set<rsl::dynamic_string>> usedIntermediateFiles{ nullptr };

        if (hasIntermediatesPath && cli.has_flag({ "clean", "c" }))
        {
            static rsl::dynamic_set<rsl::dynamic_string> usedIntermediateFilesSet;
            usedIntermediateFiles = { &usedIntermediateFilesSet };
        }

        rythe_defer_execution
        {
            if (usedIntermediateFiles)
            {
                intermediatesPath.iterate_recursive([&](rfs::view& file)
                {
                    if (!file.is_file())
                    {
                        if (file.is_directory() && file.is_empty())
                        {
                            rlog::trace("Deleting folder: \"{}\"", file.path());
                            rsl::scoped_assert_on_error noAssert(false);
                            file.delete_entry().report_errors_and_resolve();
                        }
                        return;
                    }

                    if (usedIntermediateFiles->contains(file.path()))
                    {
                        return;
                    }
                    rlog::trace("Deleting file: \"{}\"", file.path());
                    rsl::scoped_assert_on_error noAssert(false);
                    file.delete_entry(rsl::file_delete_flags::recursive).report_errors_and_resolve();
                });
            }
        };

        rsl::timer totalTimer {};
        totalTimer.start();
        rythe_defer_execution
        {
            rsl::time_span elapsedTime = totalTimer.end();
            rlog::info("File processing took {}, {} per file.", elapsedTime, elapsedTime / files.size());
        };

        for (rsl::string_view file : files)
        {
            rlog::trace("{}", file);
            rlog::indent_scope fileIndentScope{};

            rsl::timer fileTimer{};
            fileTimer.start();
            rythe_defer_execution
            {
                rsl::time_span elapsedTime = fileTimer.end();
                rlog::info("Generation took {}.", elapsedTime);
            };

            rfs::view fileView(file, true);
            rfs::view intermediateFile;
            bool saveTranslationUnit = false;
            CXTranslationUnit translationUnit = nullptr;
            if (hasIntermediatesPath)
            {
                rlog::trace("Computing content hash.");
                rsl::result<rsl::byte_view> data = fileView.read();
                if (!data.has_errors())
                {
                    rsl::hash_state hashState;
                    rsl::begin_content_hash(hashState);
                    rsl::append_content_hash(hashState, fileView.path());
                    rsl::append_content_hash(hashState, data.value());

                    const rsl::content_hash content = rsl::end_content_hash(hashState);
                    intermediateFile = intermediatesPath /
                            rsl::format("{}/{}/{}/{}/{}.rrg_ast",
                                        content.value.u32[0],
                                        content.value.u32[1],
                                        content.value.u32[2],
                                        content.value.u32[3],
                                        content.size);

                    if (usedIntermediateFiles)
                    {
                        usedIntermediateFiles->insert(intermediateFile.path());
                    }

                    rlog::trace("Loading intermediate file \"{}\".", intermediateFile.path());
                    translationUnit = load_translation_unit(index, intermediateFile);

                    saveTranslationUnit = !translationUnit;
                }
                else
                {
                    for (const rsl::error_type& error : data.get_errors())
                    {
                        rlog::trace("{}: {}", error.code, error.message);
                    }
                    rsl::log::error("Failed to load intermediate file for \"{}\", will attempt to parse from source.", file);
                    data.resolve();
                }

                fileView.release_solution();
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

            if (!translationUnit)
            {
                rsl::log::error("Failed to parse source file \"{}\", unkown error...", file);
                return -1;
            }

            if (saveTranslationUnit)
            {
                save_translation_unit(translationUnit, intermediateFile);
            }

            rsl::dynamic_string outputFileName = rsl::dynamic_string::from_view(fileView.filename());
            rsl::linear_search_and_replace(outputFileName, '.', '_');
            outputFileName.append(".hpp");

            rfs::view outputFile;
            if (outputMode == output_mode::individual)
            {
                outputFile = fileView.parent() / outputPath / outputFileName;
            }
            else
            {
                outputFile = rfs::view(outputPath) / outputFileName;
            }

            if (auto result = rrg::process_translation_unit(translationUnit, outputFile); result.has_errors())
            {
                rsl::scoped_assert_on_error noAssert(false);
                return rsl::narrowing_cast<int>(result.report_errors_and_resolve());
            }

            clang_disposeTranslationUnit(translationUnit);
        }
    }

    return 0;
}

static CXTranslationUnit load_translation_unit(CXIndex index, const rfs::view& file)
{
    if (!file.exists())
    {
        rsl::log::trace(
                "Failed to load intermediate file \"{}\" because it didn't exist, will attempt to parse from source.", file.path());
        return nullptr;
    }

    rsl::pointer<const rfs::file_solution> solution = file.get_solution();
    rsl::pointer<const rfs::local_disk_archive> nativeArchive;
    if (!solution || !(nativeArchive = { dynamic_cast<const rfs::local_disk_archive*>(solution->get_provider().ptr) }))
    {
        rsl::log::error(
                "Failed to load intermediate file \"{}\" because it was not a native file, will attempt to parse from source.",
                file.path());
        return nullptr;
    }

    CXTranslationUnit translationUnit;
    CXErrorCode error = clang_createTranslationUnit2(index, nativeArchive->get_absolute_path(*solution).data(), &translationUnit);
    if (error != CXError_Success)
    {
        rsl::log::error("Failed to load intermediate file \"{}\", will attempt to parse from source.", file.path());
        return nullptr;
    }

    return translationUnit;
}

static void save_translation_unit(CXTranslationUnit translationUnit, const rfs::view& file)
{
    rlog::trace("Saving translation unit to intermediate file.");

    if (!file.exists())
    {
        rlog::trace("Creating intermediates file \"{}\".", file.path());
        rsl::result<void> creationResult = file.create();
        if (creationResult.has_errors())
        {
            rlog::indent_scope indentScope{};
            for (const rsl::error_type& error : creationResult.get_errors())
            {
                rlog::trace("{}: {}", error.code, error.message);
            }

            creationResult.resolve();
            rlog::trace("Failed to create intermediates file \"{}\".", file.path());
            return;
        }
    }

    rsl::pointer<const rfs::file_solution> solution = file.get_solution();
    rsl::pointer<const rfs::local_disk_archive> nativeArchive;
    if (!solution || !(nativeArchive = { dynamic_cast<const rfs::local_disk_archive*>(solution->get_provider().ptr) }))
    {
        rsl::log::error("Failed to save intermediate file \"{}\" because it was not a native file.", file.path());
        return;
    }

    const int error =
            clang_saveTranslationUnit(translationUnit, nativeArchive->get_absolute_path(*solution).data(), CXSaveTranslationUnit_None);
    if (error != CXSaveError_None)
    {
        rsl::log::error("Failed to save intermediate file \"{}\".", file.path());
    }
}
