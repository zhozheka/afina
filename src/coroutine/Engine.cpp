#include <afina/coroutine/Engine.h>

#include <setjmp.h>
#include <stdio.h>
#include <string.h>
#include <iostream>

namespace Afina {
namespace Coroutine {
    void Engine::Store(context &ctx) {
        ctx.High = StackBottom;
        char stack;
        ctx.Low = &stack;

        uint32_t new_stack_size = ctx.High - ctx.Low;
        char* old_stack;
        uint32_t stack_capacity;
        std::tie(old_stack, stack_capacity) = ctx.Stack;


        if (new_stack_size > stack_capacity) {
            // allocate new memory for stack
            delete[] old_stack;

            char* new_stack = new char[new_stack_size];
            memcpy(new_stack, ctx.Low, new_stack_size);

            ctx.Stack = std::make_tuple(new_stack, new_stack_size);
        } else {
            // don't allocate new memory new_stack_size < stack_capacity
            memcpy(old_stack, ctx.Low, new_stack_size);
        }
    }

    void Engine::Restore(context &ctx) {
        char stack;
        if (&stack >= ctx.Low){
            Engine::Restore(ctx);
        }

        char* ctx_stack;
        uint32_t ctx_stack_size;
        std::tie(ctx_stack, ctx_stack_size) = ctx.Stack;

        memcpy(ctx.Low, ctx_stack, ctx_stack_size);
        longjmp(ctx.Environment, 1);
    }


    void Engine::yield() {
        context* routine = alive;

        if ((routine == cur_routine) && routine) {
            routine = routine->next;
        }

        if (routine == nullptr) {
            return;
        }
        sched(routine);

    }

    void Engine::sched(void *routine) {
        context* ctx = reinterpret_cast<context*>(routine);
        if (cur_routine) {
            if (setjmp(cur_routine->Environment)) {
                return;
            }
            Store(*cur_routine);
        }
        cur_routine = ctx;
        Restore(*cur_routine);
    }

} // namespace Coroutine
} // namespace Afina
