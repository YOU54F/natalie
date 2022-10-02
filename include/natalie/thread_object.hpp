#pragma once

#include <assert.h>
#include <ctype.h>

#include "natalie/object.hpp"

namespace Natalie {

class ThreadObject : public Object {
public:
    static constexpr size_t STACK_SIZE = 1024 * 1024; // 1 MiB

    ThreadObject()
        : Object { Object::Type::Thread, GlobalEnv::the()->Thread() } {
        GlobalEnv::the()->threads().set(this);
    }

    ThreadObject(ClassObject *klass)
        : Object { Object::Type::Thread, klass } {
        GlobalEnv::the()->threads().set(this);
    }

    ~ThreadObject() {
        GlobalEnv::the()->threads().remove(this);
    }

    static Value current();

    static ThreadObject *main() { return s_main; }
    static void build_main_thread();

    static pthread_key_t local_key(SymbolObject *key);
    static Value local_get(SymbolObject *key);
    static void local_set(SymbolObject *key, Object *value);

    Value initialize(Env *env, Block *block);

    pthread_t thread_id() const { return m_thread_id; }
    void set_thread_id(pthread_t thread_id) { m_thread_id = thread_id; }

    Block *block() { return m_block; }
    bool sleep() const { return m_sleep; }

    Value join();
    Value value();

    Value inspect(Env *env) const;

    void *start_of_stack() const { return m_start_of_stack; }
    void set_start_of_stack(void *start_of_stack) {
        assert(start_of_stack);
        m_start_of_stack = start_of_stack;
    }

    size_t stack_size() const { return m_stack_size; }
    void set_stack_size(size_t stack_size) { m_stack_size = stack_size; }

private:
    inline static ThreadObject *s_main = nullptr;
    inline static TM::Hashmap<SymbolObject *, pthread_key_t> s_local_keys {};

    pthread_t m_thread_id;
    Block *m_block;
    Object *m_value { nullptr };
    bool m_sleep { false };
    void *m_start_of_stack { nullptr };
    size_t m_stack_size { 0 };
};
}
