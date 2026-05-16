#pragma once

#include "framework_files/EmbeddedFileTable.hpp"

/**
 * Concrete EmbeddedFileTable for the application's own static files.
 *
 * Mirror of FrameworkFileTable: each file listed here has a corresponding
 * entry in AppFileTable.cpp that declares the EMBED_FILES linker symbols.
 *
 * To add a new file:
 *  1. Place it under main/app_files/files/
 *  2. Add extern declarations for its linker symbols in AppFileTable.cpp
 *  3. Add a FileEntry row in the files[] table in AppFileTable.cpp
 */
class AppFileTable : public framework_files::EmbeddedFileTable {
  public:
    static constexpr const char* TAG = "AppFileTable";

    const framework_files::EmbeddedFile* find(std::string_view path) const override;
    const uint8_t* find(const char* path, size_t& outSize) const override;
};
