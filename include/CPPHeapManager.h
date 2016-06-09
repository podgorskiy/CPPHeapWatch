#pragma once
#include <cstddef>

namespace CPPHeapWatch
{
	void SetFileAndLine(const char* fileName, unsigned int line, void* p);

	class HeapManager
	{
	public:
		static void* Alloc(size_t size, bool isArray);
		static void Free(void* pointer, bool isArray);
		static void CheckLeaks();
		static size_t GetTotallAllocatedMemory();
		static void SanityCheck();
		static void LeakSearchScope(bool enabled);
		static void PushScope(int id, const char* scopeName);
		static void PopScope(int id);
	private:
		static int m_allocCount;
		static size_t m_memoryPeak;
		static size_t m_currentMemory;
	};
	
	class Guard
	{
		struct SearchScopeTrigger
		{
			SearchScopeTrigger()
			{
#if defined CPPHEAPWATCH && !defined __EMSCRIPTEN__
				HeapManager::LeakSearchScope(true);
#endif
			}

			~SearchScopeTrigger()
			{
#if defined CPPHEAPWATCH && !defined __EMSCRIPTEN__
				HeapManager::LeakSearchScope(false);
				HeapManager::SanityCheck();
				HeapManager::CheckLeaks();
#endif
			}
		};
	public:
		Guard()
		{
			static SearchScopeTrigger trigger;
		}
	};
}
