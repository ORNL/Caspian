#pragma once
/*
 * C++ Path Library
 * Parker Mitchell, 2017-2019
 * Previously developed for a different project.
 * Modified to be header-only in this deployment.
 */

#include <cstring>
#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <sstream>
#include <iterator>
#include <algorithm>
#include <memory>

#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <dirent.h>

enum class PathType
{
    NOT_SET,
    UNKNOWN,
    NOT_FOUND,
    FILE,
    DIRECTORY,
    ROOT,
    SYMLINK
};

class Path
{
public:
    /**
     * @brief Constructor
     */
    inline Path();

    /**
     * @brief Constructor
     * @param path String with relative or absolute path
     */
    inline Path(const std::string &const_path);

    /**
     * @brief Constructor
     * @param path Vector of strings representing a relative or absolute path
     * @param is_relative true if the path is relative, false otherwise
     */
    inline Path(std::vector<std::string> &path, bool is_relative = false);

    /**
     * @brief Copy Constructor
     * @param path
     */
    inline Path(const Path &path);

    /**
     * @brief Sets a new path for the Path object
     * @param path A string representing a new path
     */
    inline void setPath(const std::string &path);

    /**
     * @brief Determines if path is a directory
     * @return true if path is a directory, false otherwise
     */
    inline bool isDir() const;

    /**
     * @brief Determines if path is a file
     * @return true if path is a file, false otherwise
     */
    inline bool isFile() const;

    /**
      * @brief Determines if path exists
     * @return true if the path exists, false otherwise
     */
    inline bool exists() const;

    /**
     * @brief Get the file name of the current path
     * @return String of the file name or an empty string if the path type is not a file
     */
    inline std::string filename() const;

    /**
     * @brief Get the file extension
     * @return File extension of current path; empty string if directory or does not exist
     */
    inline std::string extension() const;

    /**
     * @brief Moves path up a directory
     */
    inline bool upDir();

    /**
     * @brief Gets the parent directory of the current path
     * @return Path object for parent directory
     */
    inline Path getParent() const;

    /**
     * @brief Gets all of the files and directorys at the current path
     * @see Path::getChlidrenType()
     * @return A vector of path objects representing all the files/dirs at the current path
     */
    inline std::vector<Path> getChildren();

    /**
     * @brief Gets all of the sub directories at the current path
     * @see Path::getChlidrenType()
     * @return A vector of path objects representing all the dirs at the current path
     */
    inline std::vector<Path> getChildrenDirs();

    /**
     * @brief Gets all of the files at the current path
     * @see Path::getChlidrenType()
     * @return A vector of path objects representing all the files at the current path
     */
    inline std::vector<Path> getChildrenFiles();

    /**
     * @brief Gets all the symlinks at the current path
     * @return A vector of path objects representing all the symlinks at the current path
     */
    inline std::vector<Path> getChildrenSymlinks();

    /**
     * @brief Finds the specified filename recursively - DO NOT abuse
     * @param filename Name of the desired file
     * @param max_recursion Maximum levels of recursion to use when looking for the file
     * @return A path object with the file
     */
    inline std::unique_ptr<Path> find(const std::string &filename, int max_recursion);

    /**
     * @brief Removes the directory at the current path
     * @param force force the removal of the file/dir
     * @param recursive recursively remove the directory (default true)
     */
    inline void removeDir(bool recursive = true);

    /**
     * @brief Join a string to the end of the current path
     * @param path A relative path string to append to the current path
     */
    inline void join(const std::string &path);

    /**
     * @brief Overloaded += operator to append a path to the current path
     * @param rhs A string representing a relative path to append to the current path
     * @return The updated Path object
     */
    inline Path& operator+=(const std::string &rhs);

    /**
     * @brief Overloaded + operator to make a new path object when you add a string to a path
     * @param rhs A string to append to the current path
     * @return A new Path object with the updated path
     */
    inline Path operator+(const std::string &rhs);

    /**
     * @brief Resolve the file type for the path object
     * @return the type for the current path
     */
    inline PathType getType() const;

    /**
     * @brief Generates the string representation of the current path
     * @param relative Set to true to generate a relative path
     * @return A string of the current path
     */
    inline std::string str(bool relative = false) const;

    /**
     * @brief Generates a vector of strings representing the current path
     * @return A vector of strings for the current path
     */
    inline std::vector<std::string> vec() const;

    /**
     * @brief Used to determine the size of the file at the current path
     * @return Size of the file in bytes
     */
    inline size_t filesize() const;

private:

    /**
     * current path stored as a string
     */
    std::string m_path_str;

    /**
     * current path stored as a vector
     */
    std::vector<std::string> m_path_components;

    /**
     * cache a copy of the stat struct
     */
    std::unique_ptr<struct stat> m_stat;

    /**
     * classification of the current path
     */
    PathType m_path_type;

    /**
     * The size of the file in bytes
     */
    size_t m_fsize;

    /**
     * @brief Gets all the children of a certain type in the current directory
     * @param types A vector of desired types (PathType enum)
     * @see PathType
     * @return A vector of paths matching the specified type in the current directory
     */
    inline std::vector<Path> getChildrenType(const std::vector<PathType> &types);

    /**
     * @brief Updates the internal stat structure
     */
    inline void updateStat();

    /**
     * @brief Invalidates any cached data in the Path object (e.g. changed path)
     */
    inline void invalidateCache();

    /**
     * @brief Updates the internal representation of the current path
     * @param path A string representing the new path
     */
    inline void updatePath(std::string path);

    /**
     * @brief Updates the internal representation of the current path
     * @param path A vector of strings representing the new path
     */
    inline void updatePath(std::vector<std::string> &path, bool is_relative = false);

    /**
     * @brief Trims the spaces from a string
     * @param s The input string to be trimmed
     * @return The string 's' without extra spaces
     */
    inline std::string trim(const std::string& s) const;
};

inline Path::Path()
{
    // Get current working directory
    std::string path = getcwd(nullptr, 0);
    updatePath(path);
}

inline Path::Path(const std::string &const_path)
{
    std::string path = const_path;
    // Update the internal path to match
    updatePath(path);
}

inline Path::Path(std::vector<std::string> &path, bool is_relative)
{
    // Update the internal path to match
    updatePath(path, is_relative);
}

// Copy Constructor
inline Path::Path(const Path &path)
{
    // get the path string and path vector
    m_path_str = path.str();
    m_path_components = path.vec();

    // don't try to copy the stat data
    updateStat();
}

inline void Path::setPath(const std::string &path)
{
    updatePath(path);
}

inline bool Path::isDir() const
{
    return m_path_type == PathType::DIRECTORY || m_path_type == PathType::ROOT;
}

inline bool Path::isFile() const
{
    return m_path_type == PathType::FILE;
}

inline bool Path::exists() const
{
    return m_path_type != PathType::NOT_FOUND;
}

inline std::string Path::filename() const
{
    return m_path_components[m_path_components.size()-1];
}

inline std::string Path::extension() const
{
    // The path must be a file for it to have a file extension
    if(!isFile()) return "";

    // get the filename
    std::string fname = filename();

    // locate the extension by doing a reverse find for '.'
    std::string ext = fname.substr(fname.rfind('.')+1);

    return trim(ext);
}

inline Path Path::getParent() const
{
    // Get the current path as a vector while leaving off the last element
    std::vector<std::string> new_path(
            &this->m_path_components[0],
            &this->m_path_components[this->m_path_components.size()-2]
            );

    return Path(new_path, false);
}

inline std::vector<Path> Path::getChildren()
{
    std::vector<PathType> types {PathType::DIRECTORY, PathType::FILE, PathType::SYMLINK};
    return getChildrenType(types);
}

inline std::vector<Path> Path::getChildrenDirs()
{
    std::vector<PathType> types {PathType::DIRECTORY};
    return getChildrenType(types);
}

inline std::vector<Path> Path::getChildrenFiles()
{
    std::vector<PathType> types {PathType::FILE};
    return getChildrenType(types);
}

inline std::vector<Path> Path::getChildrenSymlinks()
{
    std::vector<PathType> types {PathType::SYMLINK};
    return getChildrenType(types);
}

inline std::vector<Path> Path::getChildrenType(const std::vector<PathType> &types)
{
    std::vector<Path> children;

    DIR *directory;
    struct dirent *entry;

    directory = opendir(this->str().c_str());

    if(directory == NULL)
    {
        // handle error
        std::cerr << "directory: " << str() << std::endl;
        std::cerr << "directory is null" << std::endl;
    }

    // traverse the directory
    while((entry = readdir(directory)))
    {
        int ssize = strlen(entry->d_name);

        // ignore . and ..
        if(strncmp(entry->d_name, ".",  ssize) != 0 &&
           strncmp(entry->d_name, "..", ssize) != 0   )
        {
            // make a new path object for the child
            Path p(str() + "/" + entry->d_name);

            if(std::find(types.begin(), types.end(), p.getType()) != types.end())
            {
                children.push_back(p);
            }
        }
    }

    closedir(directory);

    return children;
}

inline std::unique_ptr<Path> Path::find(const std::string &filename, int max_recursion)
{
    // base case
    if(max_recursion < 0) return nullptr;

    // get the children from the current directory
    std::vector<Path> paths = getChildren();

    // loop through each child from the current directory
    for(Path &path : paths)
    {
        if(path.isFile() && path.filename() == filename)
        {
            // if the path is a file that matches the desired name, return a pointer to it
            return std::unique_ptr<Path>(new Path(path));
        }
        else if(path.isDir())
        {
            // if the path is a directory, recursively search it
            std::unique_ptr<Path> p = path.find(filename, max_recursion-1);

            // return up the chain if we find what we are looking for
            if(p != nullptr) return p;
        }
    }

    // if we make it through the whole search and can't find it, return a nullptr
    return nullptr;
}

inline bool Path::upDir()
{
    // Can go up unless already at the root directory
    if(m_path_type != PathType::ROOT && m_path_type != PathType::UNKNOWN)
    {
        // Invalidate the cached data
        invalidateCache();

        // Remove last element
        m_path_components.pop_back();

        // Update the path
        updatePath(m_path_components);
        return true;
    }
    else
    {
        return false;
    }
}

inline void Path::join(const std::string &path)
{
    updatePath(this->str() + path);
}

inline Path& Path::operator+=(const std::string &rhs)
{
    this->join(rhs);
    return *this;
}

inline Path Path::operator+(const std::string &rhs)
{
    return Path(this->str() + rhs);
}

inline std::string Path::str(bool relative) const
{
    std::stringstream ss;

    if(relative)
    {
        std::cerr << "Relative path str is not yet supported" << std::endl;
        exit(1);
    }
    else
    {
        for(auto &segment : m_path_components)
        {
            ss << "/" << segment;
        }
    }

    return ss.str();
}

inline std::vector<std::string> Path::vec() const
{
    return m_path_components;
}

inline PathType Path::getType() const
{
    return m_path_type;
}

inline size_t Path::filesize() const
{
    return m_fsize;
}

inline void Path::updateStat()
{
    int stat_rv;
    
    // default to unknown
    m_path_type = PathType::UNKNOWN;

    // check if a stat struct should be allocated
    if(m_stat == nullptr)
    {
        m_stat.reset(new struct stat);
    }

    // get the stat for the path
    stat_rv = stat(m_path_str.c_str(), m_stat.get());

    // get the file size
    m_fsize = m_stat.get()->st_size;

    // if there was an error, determine why
    if( stat_rv != 0 )
    {
        switch(errno)
        {
            // Permissions issue
            case EACCES:
                {
                    std::cerr << "File permissions issue" << std::endl;
                    exit(1);
                    break;
                }

                // File does not exist
            case ENOTDIR:
            case ENOENT:
                {
                    m_path_type = PathType::NOT_FOUND;
                    break;
                }

                // Weird things happened
            case EBADF:
            case EFAULT:
            case ELOOP:
            case ENOMEM:
            case ENAMETOOLONG:
            case EOVERFLOW:
            default:
                {
                    std::cerr << "Unknown failure in UpdateStat" << std::endl;
                    exit(1);
                    break;
                }
        }
    }

    // Determine the type of the path (file, dir, symlink, other)
    if(m_path_type != PathType::NOT_FOUND)
    {
        switch (m_stat.get()->st_mode & S_IFMT) 
        {
            case S_IFDIR:
            {
                m_path_type = PathType::DIRECTORY;
                break;
            }

            case S_IFREG:
            {
                m_path_type = PathType::FILE;
                break;
            }

            case S_IFLNK:
            {
                m_path_type = PathType::SYMLINK;
                break;
            }

            case S_IFBLK:  // block device
            case S_IFCHR:  // char device
            case S_IFIFO:  // fifo/pipe
            case S_IFSOCK: // socket
            default:
            {
                m_path_type = PathType::UNKNOWN;
                break;
            }
        }
    }
}

inline void Path::updatePath(std::string path)
{
    std::vector<std::string> path_vec;
    std::istringstream iss;
    std::string token;
    char path_cstr[PATH_MAX];

    // Clean the path string
    std::replace(path.begin(), path.end(), '\\', '/');

    if( realpath(path.c_str(), path_cstr) == NULL )
    {
        // path probably does not exist
        m_path_str = path.c_str();
    }
    else
    {
        // save the path string
        m_path_str = path_cstr;
    }

    // clear out the old path vector
    m_path_components.clear();

    // Convert the string into a path vector
    iss.str(m_path_str);
    while(std::getline(iss, token, '/'))
    {
        if(!token.empty())
        {
            m_path_components.push_back(std::move(token));
        }
    }

    updateStat();
}

inline void Path::updatePath(std::vector<std::string> &path, bool is_relative)
{
    std::stringstream ss;
    std::string path_str;
    std::string token;

    (void) is_relative; // not needed right now

    // safety check
    if(path.empty())
    {
        std::cerr << "Empty Path variable" << std::endl;
        exit(1);
    }

    // make the relative path absolute
    std::copy(path.begin(), path.end(), std::ostream_iterator<std::string>(ss, "/"));
    ss.str(realpath(ss.str().c_str(), nullptr));

    // save the string
    m_path_str = ss.str();

    // Update the internal path vector
    while(std::getline(ss, token, '/'))
    {
        if(!token.empty())
        {
            m_path_components.push_back(std::move(token));
        }
    }

    return updateStat();
}

inline void Path::invalidateCache()
{
    m_stat.reset(nullptr);
    m_path_type = PathType::NOT_SET;
}

inline std::string Path::trim(const std::string& s) const
{
    size_t space = s.find_first_not_of(' ');
    if (space == std::string::npos)
    {
        return s;
    }
    return s.substr(space, (s.find_last_not_of(' ') - space + 1));
}
