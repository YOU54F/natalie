#include <setjmp.h>
#include <signal.h>

#include "natalie.hpp"

namespace Natalie {

void *thread_wrapper(void *arg) {
    auto thread = (ThreadObject *)arg;
    thread->set_start_of_stack(&arg);

    auto block = thread->block();
    assert(block);

    ThreadObject::local_set("NAT_current_thread"_s, thread);

    auto result = NAT_RUN_BLOCK_WITHOUT_BREAK(block->env(), block, {}, nullptr);
    return (void *)result.object();
}

Value ThreadObject::current() {
    auto thread = local_get("NAT_current_thread"_s);
    return thread->as_thread(); // let's panic if it's not a thread
}

jmp_buf stack_boundary_jump_buf;

void stack_boundary_signal_handler(int) {
    longjmp(stack_boundary_jump_buf, 1);
}

void ThreadObject::build_main_thread() {
    s_main = new ThreadObject;
    auto start_of_stack = Heap::the().start_of_stack();
    s_main->set_start_of_stack(start_of_stack);
    s_main->set_thread_id(pthread_self());

    // The most reliable way to find how large our stack *actually* is:
    // probe it with a signal handler. This is what BDWGC does.
    // Using getrlimit() to get the stack size from Linux seems like it
    // should work, but the size returned by rlim_cur is apparently
    // larger than what we can use, because if we scan past a certain
    // point (somewhere around 3K from the end), we get a segfault. :-(
    // (And that was even accounting or the fact that our "start" is
    // after args and environment variables.)
    signal(SIGSEGV, stack_boundary_signal_handler);
    char *end_of_stack = (char *)start_of_stack;
    if (setjmp(stack_boundary_jump_buf) == 0) {
        for (;;) {
            end_of_stack -= sizeof(intptr_t);
            volatile auto _deref = (*reinterpret_cast<Cell **>(end_of_stack)); // NOLINT
        }
    }
    end_of_stack += sizeof(intptr_t);
    signal(SIGSEGV, SIG_DFL);
    s_main->set_stack_size((uintptr_t)start_of_stack - (uintptr_t)end_of_stack);

    local_set("NAT_current_thread"_s, s_main);
}

pthread_key_t ThreadObject::local_key(SymbolObject *key) {
    auto hash = HashmapUtils::hashmap_hash_ptr((uintptr_t)key);
    auto item = s_local_keys.find_item(key, hash);
    if (item)
        return item->value;
    pthread_key_t pthread_key;
    auto ret = pthread_key_create(&pthread_key, nullptr);
    if (ret != 0)
        NAT_FATAL("could not create thread local storage");
    s_local_keys.put(key, pthread_key);
    return pthread_key;
}

Value ThreadObject::local_get(SymbolObject *key) {
    auto pthread_key = local_key(key);
    auto ret = (Object *)pthread_getspecific(pthread_key);
    if (!ret)
        return NilObject::the();
    return ret;
}

void ThreadObject::local_set(SymbolObject *key, Object *value) {
    auto pthread_key = local_key(key);
    auto ret = pthread_setspecific(pthread_key, value);
    if (ret != 0)
        NAT_FATAL("could not set thread local storage");
}

Value ThreadObject::initialize(Env *env, Block *block) {
    env->ensure_block_given(block);
    m_block = block;
    pthread_attr_t attr;
    auto ret = pthread_attr_init(&attr);
    if (ret != 0)
        NAT_FATAL("could not initialize thread attributes");
    m_stack_size = STACK_SIZE;
    ret = pthread_attr_setstacksize(&attr, m_stack_size);
    if (ret != 0)
        NAT_FATAL("could not set thread stack size");
    pthread_create(&m_thread_id, &attr, thread_wrapper, this);
    return this;
}

Value ThreadObject::inspect(Env *env) const {
    return StringObject::format(
        "#<Thread:{} run>",
        String::hex(m_thread_id, String::HexFormat::LowercaseAndPrefixed));
}

Value ThreadObject::join() {
    pthread_join(m_thread_id, (void **)&m_value);
    return this;
}

Value ThreadObject::value() {
    if (!m_value)
        return NilObject::the();
    return m_value;
}

}
