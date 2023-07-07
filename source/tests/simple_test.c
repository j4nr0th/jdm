//
// Created by jan on 7.7.2023.
//
#include <stdio.h>
#include <stdlib.h>
#include "../../include/jdm.h"

#ifndef NDEBUG
#ifdef __GNUC__
#define DBG_STOP __builtin_trap()
#else
#error  No dbg stop!
#endif
#else
#define DBG_STOP (void)0
#endif

int err_hook(const char* thread_name, uint32_t stack_trace_count, const char*const* stack_trace,
                                 jdm_message_level level, uint32_t line, const char* file, const char* function, const char* message, void* param)
{
    printf("Got a message from thread \"%s\" with a level of %u on line %u from file %s in function %s, that says: \"%s\"\n", thread_name, (unsigned )level, line, file, function, message);
    return 0;
}


#define ASSERT(x) if (!(x)) {fputs("Assertion \"" #x "\" failed!\n", stderr); DBG_STOP; exit(EXIT_FAILURE);} (void)0

int main()
{
    jallocator* allocator = jallocator_create(1 << 10, 1);
    ASSERT(allocator != NULL);
    ASSERT(jdm_init_thread(
            "master",
            JDM_MESSAGE_LEVEL_NONE,
            32,
            32,
            allocator
            ) == 0);
    JDM_ENTER_FUNCTION;

    jdm_set_hook(err_hook, NULL);
    JDM_ERROR("Neighbour, this isadi a wilde aopneas sdw asdioa sasdiadaw dasd dsao"
              "ij eaw asd as wdaisd nw adsioj awd si dwas jiasdis %s\n", "Cool!");

    JDM_LEAVE_FUNCTION;
    jdm_cleanup_thread();
    jallocator_destroy(allocator);
    return 0;
}

