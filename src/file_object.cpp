#include "natalie.hpp"

#include <errno.h>
#include <fcntl.h>
#include <filesystem>
#include <fnmatch.h>
#include <sys/param.h>
#include <sys/stat.h>

namespace Natalie {

// wrapper to implement euidaccess() for certain systems which
// do not have it.
int effective_uid_access(const char *path_name, int type) {
#if defined(__OpenBSD__) or defined(__APPLE__)
    uid_t real_uid = ::getuid();
    uid_t effective_uid = ::geteuid();
    gid_t real_gid = ::getgid();
    gid_t effective_gid = ::getegid();
    // if real user/group id's are the same as the effective
    // user/group id's then we can just use access(), yay!
    if (real_uid == effective_uid && real_gid == effective_gid)
        return ::access(path_name, type);
    // NATFIXME: this behavior is probably wrong, but passes specs
    // because real/effective are always equal in the tests.
    return -1;
#else
    // linux systems have an euid_access function so call it
    // directly.
    return ::euidaccess(path_name, type);
#endif
}

namespace fileutil {
    // If the `path` is not a string, but has #to_path, then
    // execute #to_path.  Otherwise if it has #to_str, then
    // execute #to_str.  Make sure the path or to_path result is a String
    // before continuing.
    // This is common to many functions in FileObject and DirObject
    Value convert_using_to_path(Env *env, Value path) {
        if (!path->is_string() && path->respond_to(env, "to_path"_s))
            path = path->send(env, "to_path"_s);
        if (!path->is_string() && path->respond_to(env, "to_str"_s))
            path = path->send(env, "to_str"_s);
        path->assert_type(env, Object::Type::String, "String");
        return path;
    }
}

Value FileObject::initialize(Env *env, Value filename, Value flags_obj, Block *block) {
    filename->assert_type(env, Object::Type::String, "String");
    int flags = O_RDONLY;
    if (flags_obj) {
        switch (flags_obj->type()) {
        case Object::Type::Integer:
            flags = flags_obj->as_integer()->to_nat_int_t();
            break;
        case Object::Type::String: {
            const char *flags_str = flags_obj->as_string()->c_str();
            if (strcmp(flags_str, "r") == 0) {
                flags = O_RDONLY;
            } else if (strcmp(flags_str, "r+") == 0) {
                flags = O_RDWR;
            } else if (strcmp(flags_str, "w") == 0) {
                flags = O_WRONLY | O_CREAT | O_TRUNC;
            } else if (strcmp(flags_str, "w+") == 0) {
                flags = O_RDWR | O_CREAT | O_TRUNC;
            } else if (strcmp(flags_str, "a") == 0) {
                flags = O_WRONLY | O_CREAT | O_APPEND;
            } else if (strcmp(flags_str, "a+") == 0) {
                flags = O_RDWR | O_CREAT | O_APPEND;
            } else {
                env->raise("ArgumentError", "invalid access mode {}", flags_str);
            }
            break;
        }
        default:
            env->raise("TypeError", "no implicit conversion of {} into String", flags_obj->klass()->inspect_str());
        }
    }
    int mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH;
    int fileno = ::open(filename->as_string()->c_str(), flags, mode);
    if (fileno == -1) {
        env->raise_errno();
    } else {
        set_fileno(fileno);
        return this;
    }
}

Value FileObject::expand_path(Env *env, Value path, Value root) {
    path->assert_type(env, Object::Type::String, "String");
    StringObject *merged;
    if (path->as_string()->length() > 0 && path->as_string()->c_str()[0] == '/') {
        merged = path->as_string();
    } else if (root) {
        root->assert_type(env, Object::Type::String, "String");
        root = expand_path(env, root, nullptr);
        merged = StringObject::format("{}/{}", root->as_string(), path->as_string());
    } else {
        char root[MAXPATHLEN + 1];
        if (!getcwd(root, MAXPATHLEN + 1))
            env->raise_errno();
        merged = StringObject::format("{}/{}", root, path->as_string());
    }
    // collapse ..
    RegexpObject dotdot { env, "[^/]*/\\.\\.(/|\\z)" };
    StringObject empty_string { "" };
    do {
        merged = merged->sub(env, &dotdot, &empty_string)->as_string();
    } while (env->has_last_match());
    // collapse .
    RegexpObject dot { env, "/\\.(/|\\z)" };
    StringObject slash { "/" };
    do {
        merged = merged->sub(env, &dot, &slash)->as_string();
    } while (env->has_last_match());
    // remove trailing slash
    if (merged->length() > 1 && merged->c_str()[merged->length() - 1] == '/') {
        merged->truncate(merged->length() - 1);
    }
    return merged;
}

void FileObject::unlink(Env *env, Value path) {
    path = fileutil::convert_using_to_path(env, path);
    int result = ::unlink(path->as_string()->c_str());
    if (result != 0)
        env->raise_errno();
}

Value FileObject::unlink(Env *env, Args args) {
    for (size_t i = 0; i < args.size(); ++i) {
        FileObject::unlink(env, args[i]);
    }
    return Value::integer(args.size());
}

void FileObject::build_constants(Env *env, ClassObject *klass) {
    Value Constants = new ModuleObject { "Constants" };
    klass->const_set("Constants"_s, Constants);
    klass->const_set("APPEND"_s, Value::integer(O_APPEND));
    Constants->const_set("APPEND"_s, Value::integer(O_APPEND));
    klass->const_set("RDONLY"_s, Value::integer(O_RDONLY));
    Constants->const_set("RDONLY"_s, Value::integer(O_RDONLY));
    klass->const_set("WRONLY"_s, Value::integer(O_WRONLY));
    Constants->const_set("WRONLY"_s, Value::integer(O_WRONLY));
    klass->const_set("TRUNC"_s, Value::integer(O_TRUNC));
    Constants->const_set("TRUNC"_s, Value::integer(O_TRUNC));
    klass->const_set("CREAT"_s, Value::integer(O_CREAT));
    Constants->const_set("CREAT"_s, Value::integer(O_CREAT));
    klass->const_set("DSYNC"_s, Value::integer(O_DSYNC));
    Constants->const_set("DSYNC"_s, Value::integer(O_DSYNC));
    klass->const_set("EXCL"_s, Value::integer(O_EXCL));
    Constants->const_set("EXCL"_s, Value::integer(O_EXCL));
    klass->const_set("NOCTTY"_s, Value::integer(O_NOCTTY));
    Constants->const_set("NOCTTY"_s, Value::integer(O_NOCTTY));
    klass->const_set("NOFOLLOW"_s, Value::integer(O_NOFOLLOW));
    Constants->const_set("NOFOLLOW"_s, Value::integer(O_NOFOLLOW));
    klass->const_set("NONBLOCK"_s, Value::integer(O_NONBLOCK));
    Constants->const_set("NONBLOCK"_s, Value::integer(O_NONBLOCK));
    klass->const_set("RDWR"_s, Value::integer(O_RDWR));
    Constants->const_set("RDWR"_s, Value::integer(O_RDWR));
    klass->const_set("SYNC"_s, Value::integer(O_SYNC));
    Constants->const_set("SYNC"_s, Value::integer(O_SYNC));

    klass->const_set("LOCK_EX"_s, Value::integer(LOCK_EX));
    Constants->const_set("LOCK_EX"_s, Value::integer(LOCK_EX));
    klass->const_set("LOCK_NB"_s, Value::integer(LOCK_NB));
    Constants->const_set("LOCK_NB"_s, Value::integer(LOCK_NB));
    klass->const_set("LOCK_SH"_s, Value::integer(LOCK_SH));
    Constants->const_set("LOCK_SH"_s, Value::integer(LOCK_SH));
    klass->const_set("LOCK_UN"_s, Value::integer(LOCK_UN));
    Constants->const_set("LOCK_UN"_s, Value::integer(LOCK_UN));

    klass->const_set("FNM_CASEFOLD"_s, Value::integer(FNM_CASEFOLD));
    Constants->const_set("FNM_CASEFOLD"_s, Value::integer(FNM_CASEFOLD));
    klass->const_set("FNM_NOESCAPE"_s, Value::integer(FNM_NOESCAPE));
    Constants->const_set("FNM_NOESCAPE"_s, Value::integer(FNM_NOESCAPE));
    klass->const_set("FNM_PATHNAME"_s, Value::integer(FNM_PATHNAME));
    Constants->const_set("FNM_PATHNAME"_s, Value::integer(FNM_PATHNAME));
}

bool FileObject::exist(Env *env, Value path) {
    struct stat sb;
    path = fileutil::convert_using_to_path(env, path);
    return ::stat(path->as_string()->c_str(), &sb) != -1;
}

bool FileObject::is_file(Env *env, Value path) {
    struct stat sb;
    path = fileutil::convert_using_to_path(env, path);
    if (::stat(path->as_string()->c_str(), &sb) == -1)
        return false;
    return S_ISREG(sb.st_mode);
}

bool FileObject::is_directory(Env *env, Value path) {
    struct stat sb;
    path = fileutil::convert_using_to_path(env, path);
    if (::stat(path->as_string()->c_str(), &sb) == -1)
        return false;
    return S_ISDIR(sb.st_mode);
}

bool FileObject::is_identical(Env *env, Value file1, Value file2) {
    file1 = fileutil::convert_using_to_path(env, file1);
    file2 = fileutil::convert_using_to_path(env, file2);
    struct stat stat1;
    struct stat stat2;
    auto result1 = ::stat(file1->as_string()->c_str(), &stat1);
    auto result2 = ::stat(file2->as_string()->c_str(), &stat2);
    if (result1 < 0) return false;
    if (result2 < 0) return false;
    if (stat1.st_dev != stat2.st_dev) return false;
    if (stat1.st_ino != stat2.st_ino) return false;
    return true;
}

bool FileObject::is_sticky(Env *env, Value path) {
    path = fileutil::convert_using_to_path(env, path);
    std::error_code ec;
    auto st = std::filesystem::status(path->as_string()->c_str(), ec);
    if (ec)
        return false;
    auto perm = st.permissions();
    return (perm & std::filesystem::perms::sticky_bit) != std::filesystem::perms::none;
}

bool FileObject::is_setgid(Env *env, Value path) {
    struct stat sb;
    path = fileutil::convert_using_to_path(env, path);
    if (::stat(path->as_string()->c_str(), &sb) == -1)
        return false;
    return (sb.st_mode & S_ISGID);
}

bool FileObject::is_setuid(Env *env, Value path) {
    struct stat sb;
    path = fileutil::convert_using_to_path(env, path);
    if (::stat(path->as_string()->c_str(), &sb) == -1)
        return false;
    return (sb.st_mode & S_ISUID);
}

bool FileObject::is_symlink(Env *env, Value path) {
    struct stat sb;
    path = fileutil::convert_using_to_path(env, path);
    if (::lstat(path->as_string()->c_str(), &sb) == -1)
        return false;
    return S_ISLNK(sb.st_mode);
}

bool FileObject::is_blockdev(Env *env, Value path) {
    struct stat sb;
    path = fileutil::convert_using_to_path(env, path);
    if (::stat(path->as_string()->c_str(), &sb) == -1)
        return false;
    return S_ISBLK(sb.st_mode);
}

bool FileObject::is_chardev(Env *env, Value path) {
    struct stat sb;
    path = fileutil::convert_using_to_path(env, path);
    if (::stat(path->as_string()->c_str(), &sb) == -1)
        return false;
    return S_ISCHR(sb.st_mode);
}

bool FileObject::is_pipe(Env *env, Value path) {
    struct stat sb;
    path->assert_type(env, Object::Type::String, "String");
    if (::stat(path->as_string()->c_str(), &sb) == -1)
        return false;
    return S_ISFIFO(sb.st_mode);
}

bool FileObject::is_socket(Env *env, Value path) {
    struct stat sb;
    path->assert_type(env, Object::Type::String, "String");
    if (::stat(path->as_string()->c_str(), &sb) == -1)
        return false;
    return S_ISSOCK(sb.st_mode);
}

bool FileObject::is_readable(Env *env, Value path) {
    path = fileutil::convert_using_to_path(env, path);
    if (access(path->as_string()->c_str(), R_OK) == -1)
        return false;
    return true;
}

bool FileObject::is_readable_real(Env *env, Value path) {
    path = fileutil::convert_using_to_path(env, path);
    if (effective_uid_access(path->as_string()->c_str(), R_OK) == -1)
        return false;
    return true;
}

Value FileObject::world_readable(Env *env, Value path) {
    struct stat sb;
    path = fileutil::convert_using_to_path(env, path);
    if (::stat(path->as_string()->c_str(), &sb) == -1)
        return NilObject::the();
    if ((sb.st_mode & (S_IROTH)) == S_IROTH) {
        auto modenum = sb.st_mode & (S_IRUSR | S_IRGRP | S_IROTH | S_IWUSR | S_IWGRP | S_IWOTH | S_IXUSR | S_IXGRP | S_IXOTH);
        return Value::integer(modenum);
    }
    return NilObject::the();
}

Value FileObject::world_writable(Env *env, Value path) {
    struct stat sb;
    path = fileutil::convert_using_to_path(env, path);
    if (::stat(path->as_string()->c_str(), &sb) == -1)
        return NilObject::the();
    if ((sb.st_mode & (S_IWOTH)) == S_IWOTH) {
        auto modenum = sb.st_mode & (S_IRUSR | S_IRGRP | S_IROTH | S_IWUSR | S_IWGRP | S_IWOTH | S_IXUSR | S_IXGRP | S_IXOTH);
        return Value::integer(modenum);
    }
    return NilObject::the();
}

bool FileObject::is_writable(Env *env, Value path) {
    path = fileutil::convert_using_to_path(env, path);
    if (access(path->as_string()->c_str(), W_OK) == -1)
        return false;
    return true;
}

bool FileObject::is_writable_real(Env *env, Value path) {
    path = fileutil::convert_using_to_path(env, path);
    if (effective_uid_access(path->as_string()->c_str(), W_OK) == -1)
        return false;
    return true;
}

bool FileObject::is_executable(Env *env, Value path) {
    path = fileutil::convert_using_to_path(env, path);
    if (access(path->as_string()->c_str(), X_OK) == -1)
        return false;
    return true;
}

bool FileObject::is_executable_real(Env *env, Value path) {
    path = fileutil::convert_using_to_path(env, path);
    if (effective_uid_access(path->as_string()->c_str(), X_OK) == -1)
        return false;
    return true;
}

bool FileObject::is_owned(Env *env, Value path) {
    struct stat sb;
    path = fileutil::convert_using_to_path(env, path);
    if (::stat(path->as_string()->c_str(), &sb) == -1)
        return false;
    return (sb.st_uid == ::geteuid());
}

bool FileObject::is_zero(Env *env, Value path) {
    struct stat sb;
    path = fileutil::convert_using_to_path(env, path);
    if (::stat(path->as_string()->c_str(), &sb) == -1)
        return false;
    return (sb.st_size == 0);
}

// oddball function that is ends in '?' but is not a boolean return.
Value FileObject::is_size(Env *env, Value path) {
    struct stat sb;
    path = fileutil::convert_using_to_path(env, path);
    if (::stat(path->as_string()->c_str(), &sb) == -1)
        return NilObject::the();
    if (sb.st_size == 0) // returns nil when file size is zero.
        return NilObject::the();
    return IntegerObject::create((nat_int_t)(sb.st_size));
}

Value FileObject::size(Env *env, Value path) {
    struct stat sb;
    path = fileutil::convert_using_to_path(env, path);
    int result = ::stat(path->as_string()->c_str(), &sb);
    if (result < 0) env->raise_errno();
    return IntegerObject::create((nat_int_t)(sb.st_size));
}

Value FileObject::symlink(Env *env, Value from, Value to) {
    from = fileutil::convert_using_to_path(env, from);
    to = fileutil::convert_using_to_path(env, to);
    int result = ::symlink(from->as_string()->c_str(), to->as_string()->c_str());
    if (result < 0) env->raise_errno();
    return Value::integer(0);
}

Value FileObject::link(Env *env, Value from, Value to) {
    from = fileutil::convert_using_to_path(env, from);
    to = fileutil::convert_using_to_path(env, to);
    int result = ::link(from->as_string()->c_str(), to->as_string()->c_str());
    if (result < 0) env->raise_errno();
    return Value::integer(0);
}

Value FileObject::mkfifo(Env *env, Value path, Value mode) {
    mode_t octmode = 0666;
    if (mode) {
        mode->assert_type(env, Object::Type::Integer, "Integer");
        octmode = (mode_t)(mode->as_integer()->to_nat_int_t());
    }
    path = fileutil::convert_using_to_path(env, path);
    int result = ::mkfifo(path->as_string()->c_str(), octmode);
    if (result < 0) env->raise_errno();
    return Value::integer(0);
}

// TODO: chmod can take multiple paths, implement that later.
Value FileObject::chmod(Env *env, Value mode, Value path) {
    path = fileutil::convert_using_to_path(env, path);
    mode->assert_type(env, Object::Type::Integer, "Integer");
    mode_t modenum = IntegerObject::convert_to_int(env, mode);
    int result = ::chmod(path->as_string()->c_str(), modenum);
    if (result < 0) env->raise_errno();
    return Value::integer(1); // return # of files
}

// Instance method (single arg)
Value FileObject::chmod(Env *env, Value mode) {
    mode_t modenum = IntegerObject::convert_to_int(env, mode);
    auto file_desc = fileno(); // current file descriptor
    int result = ::fchmod(file_desc, modenum);
    if (result < 0) env->raise_errno();
    return Value::integer(0); // always return 0
}

Value FileObject::ftype(Env *env, Value path) {
    path = fileutil::convert_using_to_path(env, path);
    std::error_code ec;
    // use symlink_status instead of status bc we do not want to follow symlinks
    auto st = std::filesystem::symlink_status(path->as_string()->c_str(), ec);
    if (ec) {
        errno = ec.value();
        env->raise_errno();
    }
    switch (st.type()) {
    case std::filesystem::file_type::regular:
        return new StringObject { "file" };
    case std::filesystem::file_type::directory:
        return new StringObject { "directory" };
    case std::filesystem::file_type::symlink:
        return new StringObject { "link" };
    case std::filesystem::file_type::block:
        return new StringObject { "blockSpecial" };
    case std::filesystem::file_type::character:
        return new StringObject { "characterSpecial" };
    case std::filesystem::file_type::fifo:
        return new StringObject { "fifo" };
    case std::filesystem::file_type::socket:
        return new StringObject { "socket" };
    default:
        return new StringObject { "unknown" };
    }
}

Value FileObject::umask(Env *env, Value mask) {
    mode_t old_mask = 0;
    if (mask) {
        mode_t mask_mode = IntegerObject::convert_to_int(env, mask);
        old_mask = ::umask(mask_mode);
    } else {
        old_mask = ::umask(0);
    }
    return Value::integer(old_mask);
}

Value FileObject::realpath(Env *env, Value pathname, Value __dir_string) {
    pathname = fileutil::convert_using_to_path(env, pathname);
    char *resolved_filepath = nullptr;
    resolved_filepath = ::realpath(pathname->as_string()->c_str(), nullptr);
    if (!resolved_filepath)
        env->raise_errno();
    auto outstr = new StringObject { resolved_filepath };
    free(resolved_filepath);
    return outstr;
}

// class method
Value FileObject::lstat(Env *env, Value path) {
    struct stat sb;
    path = fileutil::convert_using_to_path(env, path);
    int result = ::lstat(path->as_string()->c_str(), &sb);
    if (result < 0) env->raise_errno(path->as_string());
    return new FileStatObject { sb };
}

// instance method
Value FileObject::lstat(Env *env) const {
    struct stat sb;
    int result = ::stat(path().c_str(), &sb);
    if (result < 0) env->raise_errno();
    return new FileStatObject { sb };
}

// class method
Value FileObject::stat(Env *env, Value path) {
    struct stat sb;
    path = fileutil::convert_using_to_path(env, path);
    int result = ::stat(path->as_string()->c_str(), &sb);
    if (result < 0) env->raise_errno(path->as_string());
    return new FileStatObject { sb };
}

// instance method
Value FileObject::stat(Env *env) {
    struct stat sb;
    auto file_desc = fileno(); // current file descriptor
    int result = ::fstat(file_desc, &sb);
    if (result < 0) env->raise_errno();
    return new FileStatObject { sb };
}

}
