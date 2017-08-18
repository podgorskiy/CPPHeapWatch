#include <CPPHeapManager.h>

#include <vector>
#include <map>
#include <cstdio>
#include <cstdlib>
#include <csignal>
#include <cassert>
#include <mutex>
#include <cstring>

#ifdef _DEBUG
#ifdef _WIN32
#include <intrin.h>
#define BreakPoint __debugbreak
#elif defined(__linux__) || defined (__APPLE__)
#define BreakPoint __builtin_trap
#endif
#else
#define BreakPoint void
#endif

template <class T>
struct CustomAllocator 
{
	typedef T value_type;
	
	CustomAllocator() {};
	template <class Other>
	CustomAllocator(const CustomAllocator<Other>& other) {};
	
	T* allocate(std::size_t n)
	{
		return static_cast<T*>(malloc(sizeof(T) * n));
	};

	std::size_t max_size() const
	{
		return std::size_t(-1);
	}

	void deallocate(T* p, std::size_t n)
	{
		if (p != NULL)
		{
			free(p);
		}
	}
};

struct AllocationBuffer
{
	void*        m_pointer;
	size_t       m_size;
	char*        m_fileName;
	unsigned int m_line;
	bool         m_isArray;
	bool         m_ignoreIfLeaked;
	const char*  m_scopeName;
};


typedef std::map<size_t, AllocationBuffer, std::less<size_t>, CustomAllocator<AllocationBuffer> > AllocationsMap;

typedef std::vector<const char*, CustomAllocator<const char*> > MemoryScope;
typedef std::map<int, MemoryScope, std::less<int>, CustomAllocator<AllocationBuffer> > MemoryScopeMap;

int CPPHeapWatch::HeapManager::m_allocCount = 0;
size_t CPPHeapWatch::HeapManager::m_memoryPeak = 0;
size_t CPPHeapWatch::HeapManager::m_currentMemory = 0;
bool CPPHeapWatch::HeapManager::m_sanityCheck = false;
static const unsigned int k_checkCode = 0xC0DEC0DE;
static bool m_trackLeaks = false;

class AllocationsMapLock
{
public:
	AllocationsMapLock()
	{
		GetMutex().lock();
	};
	~AllocationsMapLock()
	{
		GetMutex().unlock();
	};

	static std::mutex& GetMutex()
	{
		static std::mutex allocMapMutex;
		return allocMapMutex;
	}
};

AllocationsMap& GetAllocationsMap()
{
	static AllocationsMap dataBuffers;
	return dataBuffers;
}

MemoryScopeMap& GetMemoryScopeMap()
{
	static MemoryScopeMap memoryScopeMap;
	return memoryScopeMap;
}

AllocationsMap::iterator& GetCachedIterator()
{
	static AllocationsMap::iterator cached;
	return cached;
}

void CPPHeapWatch::SetFileAndLine(const char* fileName, unsigned int line, void* p)
{
	AllocationsMapLock guard;

	AllocationsMap::iterator it;

	if (GetCachedIterator() != GetAllocationsMap().end() && GetCachedIterator()->first == (size_t)p)
	{
		it = GetCachedIterator();
	}
	else
	{
		it = GetAllocationsMap().find((size_t)p);
	}
	
	if (it != GetAllocationsMap().end())
	{
		it->second.m_fileName = static_cast<char*>(malloc(1 + strlen(fileName)));
		strcpy(it->second.m_fileName, fileName);
		it->second.m_line = line;
	}
}

void CPPHeapWatch::HeapManager::LeakSearchScope(bool enabled)
{
	m_trackLeaks = enabled;
}

void CPPHeapWatch::HeapManager::EnableSanityCheck(bool enable)
{
	m_sanityCheck = enable;
}

void* CPPHeapWatch::HeapManager::Alloc(size_t size, bool isArray)
{
	AllocationsMapLock guard;

	SanityCheck();

	int id = std::hash<std::thread::id>()(std::this_thread::get_id());

	auto scope = GetMemoryScopeMap().find(id);
	const char* scopeName = "";
	if (scope != GetMemoryScopeMap().end() && scope->second.size() > 0)
	{
		scopeName = scope->second.back();
	}

	AllocationBuffer buffer;

	buffer.m_pointer = malloc(size + sizeof(k_checkCode));
	buffer.m_fileName = nullptr;
	
	buffer.m_line = -1;
	buffer.m_size = size;
	buffer.m_isArray = isArray;
	buffer.m_ignoreIfLeaked = !m_trackLeaks;
	buffer.m_scopeName = scopeName;
	
	memcpy(static_cast<char*>(buffer.m_pointer) + size, &k_checkCode, sizeof(k_checkCode));

	auto& map = GetAllocationsMap();

	std::pair<AllocationsMap::iterator, bool> ret;
	ret = map.insert(std::pair<size_t, AllocationBuffer>((size_t)buffer.m_pointer, buffer));

	if (ret.second == false)
	{
		GetCachedIterator() = map.end();
		return nullptr;
	}

	GetCachedIterator() = ret.first;

	++m_allocCount;
	m_currentMemory += size;

	if (m_memoryPeak < m_currentMemory)
	{
		m_memoryPeak = m_currentMemory;
	}

	return buffer.m_pointer;
}

void CPPHeapWatch::HeapManager::Free(void* pointer, bool isArray)
{
	AllocationsMapLock guard;

	SanityCheck();	

	if (pointer == NULL)
	{
		return;
	}
	
	AllocationsMap::iterator it;

	auto& map = GetAllocationsMap();
	
	if (GetCachedIterator() != map.end() && GetCachedIterator()->first == (size_t)pointer)
	{
		it = GetCachedIterator();
	}
	else
	{
		it = map.find((size_t)pointer);
	}

	if (it != map.end())
	{
		if (it->second.m_pointer == pointer)
		{
			if (it->second.m_isArray != isArray)
			{
				printf("Error, undefined behavior! Allocaion was made for %s, but deletion was performed for %s\n", 
					it->second.m_isArray ? "array" : "single object", isArray ? "array" : "single object");
				printf("Allocation 0x%p: %lu bytes, at %s: %d\n", 
					it->second.m_pointer, static_cast<unsigned long>(it->second.m_size), it->second.m_fileName, it->second.m_line);
				BreakPoint();
				std::raise(SIGSEGV);
			}
			memset(it->second.m_pointer, 0x70, it->second.m_size);
			free(it->second.m_pointer);
			free(it->second.m_fileName);
			m_currentMemory -= it->second.m_size;
			map.erase(it);

			GetCachedIterator() = map.end();
			return;
		}
	}

	printf("Bad pointer deletion %p\n", pointer);
	BreakPoint();
	std::raise(SIGSEGV);
}

void CPPHeapWatch::HeapManager::CheckLeaks()
{
	AllocationsMapLock guard;

	size_t leakSize = 0;
	int annotatedLeaks = 0;
	int notAnnotatedLeaks = 0;

	printf("Memory leaks:\n\n");
	printf("Not annotated:\n");

	for (AllocationsMap::iterator it = GetAllocationsMap().begin(); it != GetAllocationsMap().end(); ++it)
	{
		if (it->second.m_ignoreIfLeaked)
		{
			continue;
		}
		if (it->second.m_fileName == nullptr)
		{
			notAnnotatedLeaks++;
			printf("0x%p: %llu bytes, scope: %s\n", it->second.m_pointer, static_cast<unsigned long long>(it->second.m_size), it->second.m_scopeName);
		}
		leakSize += it->second.m_size;
	}

	printf("\n\nAnnotated:\n");

	for (AllocationsMap::iterator it = GetAllocationsMap().begin(); it != GetAllocationsMap().end(); ++it)
	{
		if (it->second.m_ignoreIfLeaked)
		{
			continue;
		}
		if (it->second.m_fileName != nullptr)
		{
			annotatedLeaks++;
			printf("0x%p: %llu bytes, at %s: %d, scope: %s\n", it->second.m_pointer, static_cast<unsigned long long>(it->second.m_size), it->second.m_fileName, it->second.m_line, it->second.m_scopeName);
		}
		leakSize += it->second.m_size;
	}
	printf("---------------------------\n");
	printf("Total: %lu allocation, %llu bytes\n", 
		static_cast<unsigned long>(GetAllocationsMap().size()), static_cast<unsigned long long>(leakSize));
	if (leakSize > 0)
	{
		//Detected memory leaks!
		BreakPoint();
	}

	printf("Total allocation count (including all static objects): %d\n", m_allocCount);
	printf("Scoped allocation count (including only static obects that were created in the scope): %d\n", notAnnotatedLeaks + annotatedLeaks);
	printf("Annotated allocations: %d\n", annotatedLeaks);
	printf("Not annotated allocations: %d\n", notAnnotatedLeaks);
	printf("Memory peak: %llu\n", static_cast<unsigned long long>(m_memoryPeak));
	printf("Total leak (including all static objects): %llu\n", static_cast<unsigned long long>(m_currentMemory));
	printf("Scoped leak (including only static obects that were created in the scope): %llu\n", static_cast<unsigned long long>(leakSize));
}

void CPPHeapWatch::HeapManager::SanityCheck()
{
	if (!m_sanityCheck)
	{
		return;
	}
	AllocationsMapLock guard;

	int count = 0;
	for (AllocationsMap::iterator it = GetAllocationsMap().begin(); it != GetAllocationsMap().end(); ++it)
	{
		if (memcmp(static_cast<char*>(it->second.m_pointer) + it->second.m_size, &k_checkCode, sizeof(k_checkCode)) != 0)
		{
			printf("Memory corruption at 0x%p: %lu bytes, at %s: %d\n", 
				it->second.m_pointer, static_cast<unsigned long>(it->second.m_size), it->second.m_fileName, it->second.m_line);
			count++;
		}
	}

	if (count > 0)
	{
		printf("Memory corruption detected");
		BreakPoint();
		std::raise(SIGSEGV);
	}	
}

size_t CPPHeapWatch::HeapManager::GetTotallAllocatedMemory()
{
	AllocationsMapLock guard;

	size_t memory = 0;
	for (AllocationsMap::iterator it = GetAllocationsMap().begin(); it != GetAllocationsMap().end(); ++it)
	{
		memory += it->second.m_size;
	}
	return memory;
}

void CPPHeapWatch::HeapManager::PushScope(int id, const char* scopeName)
{
	AllocationsMapLock guard;

	GetMemoryScopeMap()[id].push_back(scopeName);
}

void CPPHeapWatch::HeapManager::PopScope(int id)
{
	AllocationsMapLock guard;

	GetMemoryScopeMap()[id].pop_back();
}