#include "platform/single_instance.h"

#include "core/app_identity.h"

CSingleInstanceGuard::CSingleInstanceGuard()
{
	m_hMutex = CreateMutexW(nullptr, TRUE, kSingleInstanceMutexName);
	m_bFirstInstance = m_hMutex != nullptr && GetLastError() != ERROR_ALREADY_EXISTS;
}

CSingleInstanceGuard::~CSingleInstanceGuard()
{
	Release();
}

void CSingleInstanceGuard::Release()
{
	if (m_hMutex == nullptr) return;

	if (m_bFirstInstance) {
		ReleaseMutex(m_hMutex);
	}

	CloseHandle(m_hMutex);
	m_hMutex = nullptr;
}
