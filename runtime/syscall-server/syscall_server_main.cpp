/* SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2022, eunomia-bpf org
 * All rights reserved.
 */
#include "syscall_context.hpp"
#include "bpftime_shm.hpp"
#if __linux__
#include "linux/bpf.h"
#include <asm-generic/errno-base.h>
#include <sys/epoll.h>
#endif
#include <cstdlib>
#include <cstring>
#include <spdlog/spdlog.h>
#include <unistd.h>
#include <spdlog/cfg/env.h>
#include <cstdarg>
using namespace bpftime;

// global context for bpf syscall server
static syscall_context context;

template <typename F, typename... Args>
auto handle_exceptions(F &&f, Args &&...args) noexcept
	-> decltype(f(std::forward<Args>(args)...))
{
	try {
		return f(std::forward<Args>(args)...);
	} catch (const boost::interprocess::bad_alloc &e) {
		SPDLOG_ERROR("Boost interprocess bad_alloc: {}", e.what());
		SPDLOG_ERROR("Consider increasing the shared memory size by "
			     "setting the BPFTIME_SHM_MEMORY_MB env var.");
		std::abort();
		// Terminate the program after logging the exception
	}
	// More exceptions can be added here
}

extern "C" int epoll_wait(int epfd, epoll_event *evt, int maxevents,
			  int timeout)
{
	SPDLOG_DEBUG("epoll_wait {}", epfd);
	return handle_exceptions([&]() {
		return context.handle_epoll_wait(epfd, evt, maxevents, timeout);
	});
}

extern "C" int epoll_ctl(int epfd, int op, int fd, epoll_event *evt)
{
	SPDLOG_DEBUG("epoll_ctl {} {} {} {}", epfd, op, fd, (uintptr_t)evt);
	return handle_exceptions(
		[&]() { return context.handle_epoll_ctl(epfd, op, fd, evt); });
}

extern "C" int epoll_create1(int flags)
{
	SPDLOG_DEBUG("epoll_create1 {}", flags);
	return handle_exceptions(
		[&]() { return context.handle_epoll_create1(flags); });
}

extern "C" int ioctl(int fd, unsigned long req, int data)
{
	SPDLOG_DEBUG("ioctl {} {} {}", fd, req, data);
	return handle_exceptions(
		[&]() { return context.handle_ioctl(fd, req, data); });
}

extern "C" void *mmap64(void *addr, size_t length, int prot, int flags, int fd,
			off64_t offset)
{
	SPDLOG_DEBUG("mmap64 {:x}", (uintptr_t)addr);
	return handle_exceptions([&]() {
		return context.handle_mmap64(addr, length, prot, flags, fd,
					     offset);
	});
}

extern "C" void *mmap(void *addr, size_t length, int prot, int flags, int fd,
		      off_t offset)
{
	SPDLOG_DEBUG("mmap {:x}", (uintptr_t)addr);
	return handle_exceptions([&]() {
		return context.handle_mmap(addr, length, prot, flags, fd,
					   offset);
	});
}

extern "C" int munmap(void *addr, size_t size)
{
	return handle_exceptions([&]() {
		return context.handle_munmap(addr, size);
	});
}

extern "C" int close(int fd)
{
	SPDLOG_DEBUG("Closing fd {}", fd);
	return handle_exceptions([&]() { return context.handle_close(fd); });
}

#if __linux__
extern "C" long syscall(long sysno, ...)
{
	// glibc directly reads the arguments without considering
	// the underlying argument number. So did us
	va_list args;
	va_start(args, sysno);
	long arg1 = va_arg(args, long);
	long arg2 = va_arg(args, long);
	long arg3 = va_arg(args, long);
	long arg4 = va_arg(args, long);
	long arg5 = va_arg(args, long);
	long arg6 = va_arg(args, long);
	va_end(args);
	if (sysno == __NR_bpf) {
		SPDLOG_DEBUG("SYS_BPF {} {} {} {} {} {}", arg1, arg2, arg3,
			     arg4, arg5, arg6);
		int cmd = (int)arg1;
		auto attr = (union bpf_attr *)(uintptr_t)arg2;
		auto size = (size_t)arg3;
		return handle_exceptions([&]() {
			return context.handle_sysbpf(cmd, attr, size);
		});
	} else if (sysno == __NR_perf_event_open) {
		SPDLOG_DEBUG("SYS_PERF_EVENT_OPEN {} {} {} {} {} {}", arg1,
			     arg2, arg3, arg4, arg5, arg6);
		return handle_exceptions([&]() {
			return context.handle_perfevent(
				(perf_event_attr *)(uintptr_t)arg1, (pid_t)arg2,
				(int)arg3, (int)arg4, (unsigned long)arg5);
		});
	} else if (sysno == __NR_ioctl) {
		SPDLOG_DEBUG("SYS_IOCTL {} {} {} {} {} {}", arg1, arg2, arg3,
			     arg4, arg5, arg6);
	}
	return context.orig_syscall_fn(sysno, arg1, arg2, arg3, arg4, arg5,
				       arg6);
}
#endif
