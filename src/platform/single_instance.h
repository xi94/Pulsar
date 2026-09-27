#pragma once

#include <Windows.h>

class SingleInstanceGuard {
  public:
	SingleInstanceGuard();
	~SingleInstanceGuard();

	SingleInstanceGuard(const SingleInstanceGuard &) = delete;
	SingleInstanceGuard &operator=(const SingleInstanceGuard &) = delete;

	bool is_first_instance() const
	{
		return m_first_instance;
	}

	void release();

  private:
	HANDLE m_mutex = nullptr;
	bool m_first_instance = false;
};
