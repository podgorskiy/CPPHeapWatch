#if defined CPPHEAPWATCH && !defined __EMSCRIPTEN__
#include "CPPHeapManager.h"
#include "CPPHeapWatch.h"

#pragma message("Using CPPHeapWatch!")

#undef new

using namespace CPPHeapWatch;

void* operator new(size_t size)
{
	return HeapManager::Alloc(size, false);
}

void* operator new[](size_t size)
{
	return HeapManager::Alloc(size, true);
}

void operator delete (void* pointer)
{
	return HeapManager::Free(pointer, false);
}

void operator delete[] (void* pointer)
{
	return HeapManager::Free(pointer, true);
}

#endif