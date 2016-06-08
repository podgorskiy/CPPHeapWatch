#include <CPPHeapManager.h>
#include <CPPHeapWatch.h>

#include <thread>
#include <functional>

#if defined __cplusplus && defined CPPHEAPWATCH && !defined __EMSCRIPTEN__

CPPHeapWatch::ScopeGuard::ScopeGuard(const char* scopeName)
{
	m_id = std::hash<std::thread::id>()(std::this_thread::get_id());
	CPPHeapWatch::HeapManager::PushScope(m_id, scopeName);
}

CPPHeapWatch::ScopeGuard::~ScopeGuard()
{
	CPPHeapWatch::HeapManager::PopScope(m_id);
}

#endif