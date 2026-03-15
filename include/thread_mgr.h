#pragma once
#include <stdbool.h>

void thread_mgr_init(void);
void thread_mgr_poll(void);
bool thread_mgr_is_attached(void);