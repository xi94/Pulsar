#include "platform/single_instance.h"

#include "core/app_identity.h"

SingleInstanceGuard::SingleInstanceGuard()
	: m_mutex(CreateMutexW(nullptr, TRUE, single_instance_mutex_name))
	, m_first_instance(m_mutex != nullptr && GetLastError() != ERROR_ALREADY_EXISTS)
{
}

SingleInstanceGuard::~SingleInstanceGuard()
{
	release();
}

void SingleInstanceGuard::release()
{
	if (m_mutex == nullptr) return;

	if (m_first_instance) {
		ReleaseMutex(m_mutex);
	}

	CloseHandle(m_mutex);
	m_mutex = nullptr;
}
