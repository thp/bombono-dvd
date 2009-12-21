//
// mlib/filesystem.h
// This file is part of Bombono DVD project.
//
// Copyright (c) 2009 Ilya Murav'jov
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
// 

#ifndef __MBASE_FILESYSTEM_H__
#define __MBASE_FILESYSTEM_H__

#include <boost/filesystem/path.hpp>
#include <boost/filesystem/operations.hpp>

#include <string>
#include <stdexcept> // std::runtime_error

namespace boost { namespace filesystem {

inline bool is_empty_directory(const path & dir_path)
{
    static const directory_iterator end_itr;
    return directory_iterator(dir_path) == end_itr;
}

} } // namepspace filesystem, boost
namespace fs = boost::filesystem;

std::string get_basename(const fs::path& pth);
std::string get_extension(const fs::path& pth);

namespace Project
{

fs::path MakeAbsolutePath(const fs::path& pth, const fs::path& cur_dir = fs::path());
// сделать путь pth относительным к dir
// оба аргумента должны быть абсолютными путями
bool MakeRelativeToDir(fs::path& pth, fs::path dir);
// read & write & execute
bool HaveFullAccess(const fs::path& path);
// удалить все файлы в директории
bool ClearAllFiles(const fs::path& dir_path, std::string& err_str);

} // namespace Project

std::string AppendPath(const std::string& dir, const std::string& path);

#endif // __MBASE_FILESYSTEM_H__
