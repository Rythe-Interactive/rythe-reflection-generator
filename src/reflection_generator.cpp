#include "reflection_generator.hpp"

#include <rsl/logging>

#include "class_generator.hpp"
#include "cx_string_view.hpp"
#include "field_generator.hpp"
#include "function_generator.hpp"

namespace rrg
{
    using namespace rsl::literals;

    namespace
    {
        struct scope
        {
            cx_string_view spelling;
            bool isGenerated;
        };

        struct generator_context
        {
            rsl::dynamic_string& contentBuffer;
            rsl::result<void> result;
            rsl::dynamic_array<scope>& scopes;
        };

        bool has_reflect_attribute(CXCursor cursor)
        {
            bool result = false;
            clang_visitChildren(
                    cursor,
                    [](CXCursor cursor, CXCursor, CXClientData ctx)
            {
                if (clang_isAttribute(clang_getCursorKind(cursor)))
                {
                    if (cx_string_view(clang_getCursorSpelling(cursor)).value() == "rsl_reflect_attr"_sv)
                    {
                        (*static_cast<bool*>(ctx)) = true;
                        return CXChildVisit_Break;
                    }
                    return CXChildVisit_Continue;
                }

                return CXChildVisit_Break;
            },
                    &result);

            return result;
        }

        void generate_scopes(rsl::dynamic_string& contentBuffer, rsl::array_view<scope> scopes)
        {
            if (scopes.is_empty())
            {
                return;
            }

            for (auto& scope : scopes)
            {
                if (!scope.isGenerated)
                {
                    rsl::format_to(contentBuffer, "namespace: \"{}\"\n", scope.spelling.value());
                    scope.isGenerated = true;
                }
            }
        }

        CXChildVisitResult visit_translation_unit(CXCursor cursor, rsl::pointer<generator_context> context)
        {
            if (!clang_Location_isFromMainFile(clang_getCursorLocation(cursor)))
            {
                return CXChildVisit_Continue;
            }

            CXCursorKind kind = clang_getCursorKind(cursor);

            switch (kind)
            {
                case CXCursor_ClassDecl:
                case CXCursor_StructDecl:
                {
                    if (has_reflect_attribute(cursor))
                    {
                        generate_scopes(context->contentBuffer, context->scopes);

                        if (rsl::result<void> result = generate_class(context->contentBuffer, cursor); result.has_errors())
                        {
                            context->result = result.propagate();
                            return CXChildVisit_Break;
                        }
                    }
                    break;
                }
                case CXCursor_FieldDecl:
                {
                    if (has_reflect_attribute(cursor))
                    {
                        generate_scopes(context->contentBuffer, context->scopes);

                        if (rsl::result<void> result = generate_field(context->contentBuffer, cursor); result.has_errors())
                        {
                            context->result = result.propagate();
                            return CXChildVisit_Break;
                        }
                    }
                    break;
                }
                case CXCursor_FunctionDecl:
                {
                    if (has_reflect_attribute(cursor))
                    {
                        generate_scopes(context->contentBuffer, context->scopes);

                        if (rsl::result<void> result = generate_function(context->contentBuffer, cursor); result.has_errors())
                        {
                            context->result = result.propagate();
                            return CXChildVisit_Break;
                        }
                    }
                    break;
                }
                default:
                {
                    context->scopes.push_back({ .spelling = cx_string_view(clang_getCursorSpelling(cursor)), .isGenerated = false });

                    clang_visitChildren(cursor, [](CXCursor cursor, CXCursor, CXClientData ctx) {
                        return visit_translation_unit(cursor, { static_cast<generator_context*>(ctx) });
                    }, context.ptr);

                    context->scopes.pop_back();

                    if (context->result.has_errors())
                    {
                        return CXChildVisit_Break;
                    }
                    break;
                }
            }

            return CXChildVisit_Continue;
        }

        rsl::result<void> write_reflection_file_header(
                rsl::dynamic_string& contentBuffer, const rfs::view& sourceFile, const rfs::view& outputFile)
        {
            rsl::pointer<const rfs::file_solution> solution = sourceFile.get_solution();
            rsl::pointer<const rfs::local_disk_archive> nativeArchive;
            if (!solution || !(nativeArchive = { dynamic_cast<const rfs::local_disk_archive*>(solution->get_provider().ptr) }))
            {
                return rsl::make_error(rsl::filesystem_error::invalid_solution, "Source file is not a native file.");
            }

            rsl::format_to(
                    contentBuffer,
                    "#pragma once\n#include<rsl/reflection>\n#include\"{}\"\nnamespace rythe::reflection{{void "
                    "report_reflection_data_{}(rrfl::reflection_registry& registry){{",
                    nativeArchive->get_absolute_path(*solution),
                    rfs::strip_extension(outputFile.filename()));

            return rsl::okay;
        }
    } // namespace

    rsl::result<void> process_translation_unit(
            CXTranslationUnit translationUnit, const rfs::view& sourceFile, rfs::view& outputFile, rsl::dynamic_string& contentBuffer)
    {
        contentBuffer.clear();

        CXCursor cursor = clang_getTranslationUnitCursor(translationUnit);

        if (outputFile.exists())
        {
            if (outputFile.exists())
            {
                if (rsl::result<void> result = outputFile.delete_entry(); result.has_errors())
                {
                    rlog::trace("Failed to delete file \"{}\".", outputFile.path());
                    return result.propagate();
                }
            }
        }

        rlog::trace("Creating file \"{}\".", outputFile.path());
        if (rsl::result<void> result = outputFile.create(); result.has_errors())
        {
            return result.propagate();
        }

        if (rsl::result<void> result = write_reflection_file_header(contentBuffer, sourceFile, outputFile); result.has_errors())
        {
            return result.propagate();
        }

        rsl::dynamic_array<scope> scopes;
        scopes.reserve(4ull);
        generator_context context{ .contentBuffer = contentBuffer, .result = {}, .scopes = scopes };
        clang_visitChildren(cursor, [](CXCursor cursor, CXCursor, CXClientData ctx) {
            return visit_translation_unit(cursor, { static_cast<generator_context*>(ctx) });
        }, &context);

        if (context.result.has_errors())
        {
            return context.result.propagate();
        }

        contentBuffer += "}}"_sv;

        if (rsl::result<void> result = outputFile.write(contentBuffer.view()); result.has_errors())
        {
            return result.propagate();
        }

        return rsl::okay;
    }
} // namespace rrg
