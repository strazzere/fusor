pass:
	@cd build && cmake .. && make -j$(nproc)

opt: rebuild
	@opt-3.8 -load build/fusor/libFusorPass.so -fusor example.ll | llvm-dis-3.8 | tee out.ll

cfg: opt
	@opt-3.8 -dot-cfg out.ll > /dev/null

assembly:
	@llc-3.8 out.ll && clang out.s

rebuild: examples/example.c pass
	@clang -S -emit-llvm examples/example.c
	@clang -c -emit-llvm examples/example.c

full: pass examples/example.c
	@clang -Xclang -load -Xclang build/fusor/libFusorPass.so examples/example.c

test: pass
	@for dir in $$(ls tests); do \
		echo "\n================ Testing $$dir ================"; \
		cd tests/$$dir && make >/dev/null 2>/dev/null; \
		./$$dir > $$dir.txt && ./fusor > fusor.txt;\
		if [ $$(diff $$dir.txt fusor.txt | wc -c) != 0 ]; then \
			echo "---------------- $$dir FAIL ----------------";\
		else \
			echo "---------------- $$dir PASS ----------------";\
		fi; \
		make clean >/dev/null 2>/dev/null; \
		cd ../..; \
	done;

pin: pin/proccount.cpp
	cd pin && make

clean:
	rm -f *.bc *.ll *.out *.s *.dot

debug: log.c pass examples/example.c
	@clang -c log.c
	@clang -Xclang -load -Xclang build/fusor/libFusorPass.so -c examples/example.c
	@clang log.o example.o
	@rm example.o
