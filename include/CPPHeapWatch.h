#pragma once
#if defined __cplusplus && defined CPPHEAPWATCH && !defined __EMSCRIPTEN__

#ifdef new
#undef new
#endif

void* operator new(size_t size);

void* operator new[](size_t size);

void operator delete (void* pointer);

void operator delete[](void* pointer);

namespace CPPHeapWatch
{
	class ScopeGuard
	{
	public:
		ScopeGuard(const char* scopeName);
		~ScopeGuard();
	private:
		int m_id;
	};

	void SetFileAndLine(const char* fileName, unsigned int line, void* p);

	class SetFileAndLineHelper
	{
	public:
		SetFileAndLineHelper(const char* fileName, unsigned int line): fileName(fileName), line(line)
		{
		}

		template<typename T>
		T* operator << (T* p)
		{
			CPPHeapWatch::SetFileAndLine(fileName, line, p);
			return p;
		};
	private:
		const char* fileName;
		unsigned int line;
	};
}

#define new CPPHeapWatch::SetFileAndLineHelper(__FILE__, __LINE__) << new

#define CPPHeapWatchScope(X) CPPHeapWatch::ScopeGuard scopeGuard_##X(#X);

#else

#define CPPHeapWatchScope(X)

#endif