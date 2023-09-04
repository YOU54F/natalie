#include "natalie.hpp"

#include <fcntl.h>
#include <limits.h>
#include <math.h>
#include <sys/ioctl.h>
#include <unistd.h>

namespace Natalie {

static inline bool is_readable(const int fd) {
    const int flags = fcntl(fd, F_GETFL);
    return (flags & (O_RDONLY | O_WRONLY | O_RDWR)) != O_WRONLY;
}

static inline bool is_writable(const int fd) {
    const int flags = fcntl(fd, F_GETFL);
    return (flags & (O_RDONLY | O_WRONLY | O_RDWR)) != O_RDONLY;
}

static void throw_unless_readable(Env *env, const IoObject *const self) {
    // read(2) assigns EBADF to errno if not readable, we want an IOError instead
    const auto old_errno = errno;
    if (!is_readable(self->fileno(env)))
        env->raise("IOError", "not opened for reading");
    errno = old_errno; // errno may have been changed by fcntl, revert to the old value
    env->raise_errno();
}

static void throw_unless_writable(Env *env, const IoObject *const self) {
    // write(2) assigns EBADF to errno if not writable, we want an IOError instead
    const auto old_errno = errno;
    if (!is_writable(self->fileno(env)))
        env->raise("IOError", "not opened for writing");
    errno = old_errno; // errno may have been changed by fcntl, revert to the old value
    env->raise_errno();
}

Value IoObject::initialize(Env *env, Value file_number) {
    file_number->assert_type(env, Object::Type::Integer, "Integer");
    nat_int_t fileno = file_number->as_integer()->to_nat_int_t();
    assert(fileno >= INT_MIN && fileno <= INT_MAX);
    set_fileno(fileno);
    return this;
}

void IoObject::raise_if_closed(Env *env) const {
    if (m_closed) env->raise("IOError", "closed stream");
}

Value IoObject::advise(Env *env, Value advice, Value offset, Value len) {
    raise_if_closed(env);
    advice->assert_type(env, Object::Type::Symbol, "Symbol");
    nat_int_t offset_i = (offset == nullptr) ? 0 : IntegerObject::convert_to_nat_int_t(env, offset);
    nat_int_t len_i = (len == nullptr) ? 0 : IntegerObject::convert_to_nat_int_t(env, len);
    int advice_i = 0;
#ifdef __linux__
    if (advice == "normal"_s) {
        advice_i = POSIX_FADV_NORMAL;
    } else if (advice == "sequential"_s) {
        advice_i = POSIX_FADV_SEQUENTIAL;
    } else if (advice == "random"_s) {
        advice_i = POSIX_FADV_RANDOM;
    } else if (advice == "noreuse"_s) {
        advice_i = POSIX_FADV_NOREUSE;
    } else if (advice == "willneed"_s) {
        advice_i = POSIX_FADV_WILLNEED;
    } else if (advice == "dontneed"_s) {
        advice_i = POSIX_FADV_DONTNEED;
    } else {
        env->raise("NotImplementedError", "Unsupported advice: {}", advice);
    }
    if (::posix_fadvise(m_fileno, offset_i, len_i, advice_i) != 0)
        env->raise_errno();
#else
    if (advice != "normal"_s && advice != "sequential"_s && advice != "random"_s && advice != "noreuse"_s && advice != "willneed"_s && advice != "dontneed"_s) {
        env->raise("NotImplementedError", "Unsupported advice: {}", advice->as_symbol()->string());
    }
#endif
    return NilObject::the();
}

Value IoObject::each_byte(Env *env, Block *block) {
    if (block == nullptr)
        return send(env, "enum_for"_s, { "each_byte"_s });

    Value byte;
    while (!(byte = getbyte(env))->is_nil())
        NAT_RUN_BLOCK_AND_POSSIBLY_BREAK(env, block, { byte }, nullptr);

    return this;
}

int IoObject::fileno() const {
    return m_fileno;
}

int IoObject::fileno(Env *env) const {
    raise_if_closed(env);
    return m_fileno;
}

Value IoObject::fcntl(Env *env, Value cmd_value, Value arg_value) {
    raise_if_closed(env);
    const auto cmd = IntegerObject::convert_to_int(env, cmd_value);
    int result;
    if (arg_value == nullptr || arg_value->is_nil()) {
        result = ::fcntl(m_fileno, cmd);
    } else if (arg_value->is_string()) {
        const auto arg = arg_value->as_string()->c_str();
        result = ::fcntl(m_fileno, cmd, arg);
    } else {
        const auto arg = IntegerObject::convert_to_int(env, arg_value);
        result = ::fcntl(m_fileno, cmd, arg);
    }
    if (result < 0) env->raise_errno();
    return IntegerObject::create(result);
}

int IoObject::fdatasync(Env *env) {
    raise_if_closed(env);
#ifdef __linux__
    if (::fdatasync(m_fileno) < 0) env->raise_errno();
#endif
    return 0;
}

int IoObject::fsync(Env *env) {
    raise_if_closed(env);
    if (::fsync(m_fileno) < 0) env->raise_errno();
    return 0;
}

Value IoObject::getbyte(Env *env) {
    raise_if_closed(env);
    unsigned char buffer;
    int result = ::read(m_fileno, &buffer, 1);
    if (result == -1) throw_unless_readable(env, this);
    if (result == 0) return NilObject::the(); // eof case
    return Value::integer(buffer);
}

bool IoObject::is_eof(Env *env) {
    raise_if_closed(env);
    if (!is_readable(m_fileno))
        env->raise("IOError", "not opened for reading");
    size_t buffer = 0;
    if (::ioctl(m_fileno, FIONREAD, &buffer) < 0)
        env->raise_errno();
    return buffer == 0;
}

bool IoObject::isatty(Env *env) const {
    raise_if_closed(env);
    return ::isatty(m_fileno) == 1;
}

Value IoObject::read_file(Env *env, Value filename, Value length, Value offset) {
    ClassObject *File = GlobalEnv::the()->Object()->const_fetch("File"_s)->as_class();
    FileObject *file = _new(env, File, { filename }, nullptr)->as_file();
    if (offset && !offset->is_nil())
        file->set_pos(env, offset);
    auto data = file->read(env, length);
    file->close(env);
    return data;
}

Value IoObject::write_file(Env *env, Value filename, Value string) {
    Value flags = new StringObject { "w" };
    ClassObject *File = GlobalEnv::the()->Object()->const_fetch("File"_s)->as_class();
    FileObject *file = _new(env, File, { filename, flags }, nullptr)->as_file();
    int bytes_written = file->write(env, string);
    file->close(env);
    return Value::integer(bytes_written);
}

#define NAT_READ_BYTES 1024

Value IoObject::read(Env *env, Value count_value) const {
    raise_if_closed(env);
    ssize_t bytes_read;
    if (count_value && !count_value->is_nil()) {
        count_value->assert_type(env, Object::Type::Integer, "Integer");
        int count = count_value->as_integer()->to_nat_int_t();
        if (count < 0)
            env->raise("ArgumentError", "negative length {} given", count);
        char *buf = new char[count + 1];
        auto buf_cleanup = Defer([&]() { delete[] buf; });
        bytes_read = ::read(m_fileno, buf, count);
        if (bytes_read < 0) {
            env->raise_errno();
        } else if (bytes_read == 0) {
            if (count == 0)
                return new StringObject {};
            return NilObject::the();
        } else {
            Value result = new StringObject { buf, static_cast<size_t>(bytes_read) };
            return result;
        }
    }
    char buf[NAT_READ_BYTES + 1];
    bytes_read = ::read(m_fileno, buf, NAT_READ_BYTES);
    if (bytes_read < 0) {
        env->raise_errno();
    } else if (bytes_read == 0) {
        return new StringObject { "" };
    }
    StringObject *str = new StringObject { buf, static_cast<size_t>(bytes_read) };
    while (1) {
        bytes_read = ::read(m_fileno, buf, NAT_READ_BYTES);
        if (bytes_read < 0) env->raise_errno();
        if (bytes_read == 0) break;
        buf[bytes_read] = 0;
        str->append(buf, bytes_read);
    }
    return str;
}

Value IoObject::append(Env *env, Value obj) {
    raise_if_closed(env);
    if (!obj->is_string() && obj->respond_to(env, "to_str"_s))
        obj = obj.send(env, "to_s"_s);
    obj->assert_type(env, Object::Type::String, "String");
    auto result = ::write(m_fileno, obj->as_string()->c_str(), obj->as_string()->length());
    if (result == -1) env->raise_errno();
    return this;
}

int IoObject::write(Env *env, Value obj) const {
    raise_if_closed(env);
    obj = obj->to_s(env);
    obj->assert_type(env, Object::Type::String, "String");
    int result = ::write(m_fileno, obj->as_string()->c_str(), obj->as_string()->length());
    if (result == -1) throw_unless_writable(env, this);
    return result;
}

Value IoObject::write(Env *env, Args args) const {
    args.ensure_argc_at_least(env, 1);
    int bytes_written = 0;
    for (size_t i = 0; i < args.size(); i++) {
        bytes_written += write(env, args[i]);
    }
    return Value::integer(bytes_written);
}

// NATFIXME: Make this spec compliant and maybe more performant?
Value IoObject::gets(Env *env) const {
    raise_if_closed(env);
    char buffer[NAT_READ_BYTES + 1];
    size_t index;
    for (index = 0; index < NAT_READ_BYTES; ++index) {
        if (::read(m_fileno, &buffer[index], 1) == 0)
            return NilObject::the();

        if (buffer[index] == '\n')
            break;
    }
    auto line = new StringObject { buffer, index + 1 };
    env->set_last_line(line);
    return line;
}

Value IoObject::get_path() const {
    if (m_path == nullptr)
        return NilObject::the();
    return new StringObject { *m_path };
}

void IoObject::putstr(Env *env, StringObject *str) {
    String sstr = str->string();
    this->send(env, "write"_s, Args({ str }));
    if (sstr.size() == 0 || (sstr.size() > 0 && !sstr.ends_with("\n"))) {
        this->send(env, "write"_s, Args({ new StringObject { "\n" } }));
    }
}

void IoObject::putary(Env *env, ArrayObject *ary) {
    for (auto &item : *ary) {
        this->puts(env, item);
    }
}

void IoObject::puts(Env *env, Value val) {
    if (val->is_string()) {
        this->putstr(env, val->as_string());
    } else if (val->respond_to(env, "to_ary"_s)) {
        auto ary = val->send(env, "to_ary"_s);
        ary->assert_type(env, Object::Type::Array, "Array");
        this->putary(env, ary->as_array());
    } else {
        Value str = val->send(env, "to_s"_s);
        if (str->is_string()) {
            this->putstr(env, str->as_string());
        } else { // to_s did not return a string to inspect val instead.
            this->putstr(env, new StringObject { val->inspect_str(env) });
        }
    }
}

Value IoObject::puts(Env *env, Args args) {
    if (args.size() == 0) {
        this->send(env, "write"_s, Args({ new StringObject { "\n" } }));
    } else {
        for (size_t i = 0; i < args.size(); i++) {
            this->puts(env, args[i]);
        }
    }
    return NilObject::the();
}

Value IoObject::print(Env *env, Args args) const {
    if (args.size() > 0) {
        auto fsep = env->output_file_separator();
        auto valid_fsep = !fsep->is_nil();
        for (size_t i = 0; i < args.size(); i++) {
            if (i > 0 && valid_fsep)
                write(env, fsep);
            write(env, args[i]);
        }
    } else {
        Value lastline = env->last_line();
        write(env, lastline);
    }
    auto rsep = env->output_record_separator();
    if (!rsep->is_nil()) write(env, rsep);
    return NilObject::the();
}

Value IoObject::close(Env *env) {
    if (m_closed)
        return NilObject::the();
    int result = ::close(m_fileno);
    if (result == -1) {
        env->raise_errno();
    } else {
        m_closed = true;
        return NilObject::the();
    }
}

Value IoObject::seek(Env *env, Value amount_value, Value whence_value) const {
    raise_if_closed(env);
    nat_int_t amount = IntegerObject::convert_to_nat_int_t(env, amount_value);
    int whence = 0;
    if (whence_value) {
        switch (whence_value->type()) {
        case Object::Type::Integer:
            whence = whence_value->as_integer()->to_nat_int_t();
            break;
        case Object::Type::Symbol: {
            SymbolObject *whence_sym = whence_value->as_symbol();
            if (whence_sym->string() == "SET") {
                whence = 0;
            } else if (whence_sym->string() == "CUR") {
                whence = 1;
            } else if (whence_sym->string() == "END") {
                whence = 2;
            } else {
                env->raise("TypeError", "no implicit conversion of Symbol into Integer");
            }
            break;
        }
        default:
            env->raise("TypeError", "no implicit conversion of {} into Integer", whence_value->klass()->inspect_str());
        }
    }
    int result = lseek(m_fileno, amount, whence);
    if (result == -1)
        env->raise_errno();
    return Value::integer(0);
}

Value IoObject::stat(Env *env) const {
    struct stat sb;
    auto file_desc = fileno(env); // current file descriptor
    int result = ::fstat(file_desc, &sb);
    if (result < 0) env->raise_errno();
    return new FileStatObject { sb };
}

Value IoObject::sysopen(Env *env, Value path, Value flags_obj, Value perm) {
    const auto flags = fileutil::flags_obj_to_flags(env, nullptr, flags_obj);
    const auto modenum = fileutil::perm_to_mode(env, perm);

    path = fileutil::convert_using_to_path(env, path);
    const auto fd = ::open(path->as_string()->c_str(), flags, modenum);
    return IntegerObject::create(fd);
}

IoObject *IoObject::to_io(Env *env) {
    return this->as_io();
}

Value IoObject::try_convert(Env *env, Value val) {
    if (val->is_io()) {
        return val;
    } else if (val->respond_to(env, "to_io"_s)) {
        val = val->send(env, "to_io"_s);
        val->assert_type(env, Object::Type::Io, "IO");
        return val;
    }
    return NilObject::the();
}

int IoObject::rewind(Env *env) {
    raise_if_closed(env);
    errno = 0;
    auto result = ::lseek(m_fileno, 0, SEEK_SET);
    if (result < 0 && errno) env->raise_errno();
    return 0;
}

int IoObject::set_pos(Env *env, Value position) {
    raise_if_closed(env);
    nat_int_t offset = IntegerObject::convert_to_nat_int_t(env, position);
    errno = 0;
    auto result = ::lseek(m_fileno, offset, SEEK_SET);
    if (result < 0 && errno) env->raise_errno();
    return result;
}

int IoObject::pos(Env *env) {
    raise_if_closed(env);
    errno = 0;
    auto result = ::lseek(m_fileno, 0, SEEK_CUR);
    if (result < 0 && errno) env->raise_errno();
    return result;
}

// This is a variant of getbyte that raises EOFError
Value IoObject::readbyte(Env *env) {
    auto result = getbyte(env);
    if (result->is_nil())
        env->raise("EOFError", "end of file reached");
    return result;
}

// This is a variant of gets that raises EOFError
// NATFIXME: Add arguments and chomp kwarg when those features are
//  added to IOObject::gets()
Value IoObject::readline(Env *env) const {
    auto result = gets(env);
    if (result->is_nil())
        env->raise("EOFError", "end of file reached");
    return result;
}

}
