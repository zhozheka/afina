#include <afina/coroutine/Engine.h>

#include <setjmp.h>
#include <stdio.h>
#include <string.h>
#include <iostream>

namespace Afina {
namespace Coroutine {
    void Engine::Store(context &ctx) {
        char stack;
        ctx.Low = StackBottom;
        ctx.High = StackBottom;
        if (ctx.Low > &stack){
            ctx.Low = &stack;
        }
        else {
            ctx.High = &stack;
        }
        uint32_t new_stack_size = ctx.High - ctx.Low;

        char* old_stack;
        uint32_t old_stack_size;
        std::tie(old_stack, old_stack_size) = ctx.Stack;


        if (new_stack_size > old_stack_size) {
            delete[] old_stack;

            char* new_stack = new char[new_stack_size];
            ctx.Stack = std::make_tuple(new_stack, new_stack_size);
            memcpy(new_stack, ctx.Low, new_stack_size);
        } else {
            memcpy(old_stack, ctx.Low, old_stack_size);
        }

    }

    void Engine::Restore(context &ctx) {
        char stack;
        if ((&stack >= ctx.Low) && (&stack <= ctx.High)){
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

        if (routine == cur_routine && routine) {
            routine = routine->next;
        }

        if (routine) {
            sched(routine);
        } else {
            return;
        }
    }

    void Engine::sched(void *routine_) {
        context* ctx = reinterpret_cast<context*>(routine_);
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
