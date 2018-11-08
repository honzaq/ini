#pragma once

#include "file_watcher_intf.h"
#include <memory>
#include <map>
#include <mutex>

namespace watcher {

// reflect windows.h HANDLE
using win_handle = void*;

// 
class file : public file_intf
{
public:

	struct factory {
		virtual std::unique_ptr<watcher::file_intf> create_file_watch();
	};

	//////////////////////////////////////////////////////////////////////////
	std::shared_ptr<registration::registrator_intf> subscribe(const std::wstring& file_path, on_file_changed_intf& event_handler) override;
protected:
	void initialize();
	void test_fn();

private:
	win_handle                                                      m_stop_event = nullptr;
	win_handle                                                      m_new_file = nullptr;
	std::mutex                                                      m_registrations_lock;
	std::map<std::wstring, registration::holder_void<std::wstring>> m_registrations;
};

} // end of namespace watcher