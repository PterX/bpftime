#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <llvm/llvm_jit_context.hpp>
#include <llvm_bpf_jit.h>
#include <ebpf-vm.h>
#include <linux/filter.h>

extern "C" uint64_t add_func(uint64_t a, uint64_t b, uint64_t, uint64_t,
			     uint64_t)
{
	return a + b;
}

TEST_CASE("Test aot compilation")
{
	ebpf_vm *vm = ebpf_create();
	REQUIRE(vm != nullptr);

	REQUIRE(ebpf_register(vm, 1, "add", (void *)add_func) == 0);

	struct bpf_insn insns[] = { BPF_MOV64_IMM(1, 123),
				    BPF_MOV64_IMM(2, 1000), BPF_EMIT_CALL(1),
				    BPF_EXIT_INSN() };
	char *errmsg = nullptr;
	REQUIRE(ebpf_load(vm, (const void *)insns, sizeof(insns), &errmsg) ==
		0);
	REQUIRE(errmsg == nullptr);
	uint64_t ret = 0;
	uint64_t mem = 0;
	SECTION("Run using JIT")
	{
		REQUIRE(ebpf_exec(vm, &mem, sizeof(mem), &ret) == 0);
		REQUIRE(ret == 123 + 1000);
	}
	SECTION("Run using AOT")
	{
		auto aot_result = vm->jit_context->do_aot_compile();
		REQUIRE(aot_result.size() > 0);
		vm->jit_context->load_aot_object(aot_result);
		ret = vm->jit_context->get_entry_address()(&mem, sizeof(mem));
		REQUIRE(ret == 123 + 1000);
	}

	ebpf_destroy(vm);
}
