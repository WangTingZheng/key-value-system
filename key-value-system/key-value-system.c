#include<stdio.h>
#include <dlfcn.h>
#define SOPATH "../out/build/WSL-GCC-Debug/key-value-system/libkvs.so" 

MyEnum myEnum;
int main() {
	void* handle = dlopen(SOPATH, RTLD_LAZY);
	return 0;
}