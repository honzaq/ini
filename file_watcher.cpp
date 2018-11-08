#include "file_watcher.h"
#include <filesystem>
#include <windows.h>
#include "scope_guard.h"

namespace fs = std::experimental::filesystem::v1;

namespace watcher {

std::unique_ptr<file_intf> file::factory::create_file_watch()
{
	auto sp_impl = std::make_unique<file>();
	sp_impl->initialize();
	return sp_impl;
}

void file::initialize()
{
	m_stop_event = ::CreateEvent(nullptr, TRUE, FALSE, nullptr);
	if(!m_stop_event) {
		throw std::exception("Cannot properly initialize (stop event)");
	}
	m_new_file = ::CreateEvent(nullptr, FALSE, FALSE, nullptr);
	if(!m_new_file) {
		throw std::exception("Cannot properly initialize (stop event)");
	}
}

std::shared_ptr<registration::registrator_intf> file::subscribe(const std::wstring& file_path, on_file_changed_intf& event_handler)
{
	try {
		std::lock_guard<std::mutex> lock(m_registrations_lock);
		bool new_item = false;
		if(m_registrations.find(file_path) == m_registrations.end()) {
			new_item = true;
		}
		auto sp_reg = m_registrations[file_path].subscribe(
			// notify callback
			[&event_handler](const std::wstring& changed_file_path) {
				event_handler.on_change(changed_file_path);
			},
			// unregister callback
			[&file_path, this]() {
				std::lock_guard<std::mutex> lock(m_registrations_lock);
				SetEvent(m_remove_file);
// 				const auto& item = m_registrations.find(file_path);
// 				if(item != m_registrations.end() && item->second.size() == 1) {
// 					m_registrations.erase(item);
// 				}
			}
		);
		if(new_item) {
			::SetEvent(m_new_file);
		}
		return sp_reg;
	} catch(const std::exception& ex) {
		printf("Register callback failed with exception, reason: %s\n", ex.what());
		throw;
	}
}

void file::test_fn()
{
	std::wstring file_path;

	fs::path path(file_path);

	std::wstring dir = path.parent_path();

// 	std::vector<win_handle> change_handles;
// 	change_handles.resize(2);
// 	change_handles[0] = m_stop_event;
// 	change_handles[1] = m_new_file;

	HANDLE change_handles[2] = { 0 };
	// Stop event
	change_handles[0] = m_stop_event;
	// Watch the directory for file creation and deletion. 
	change_handles[1] = ::FindFirstChangeNotification(
		dir.c_str(),                    // directory to watch 
		FALSE,                          // do not watch subtree 
		FILE_NOTIFY_CHANGE_LAST_WRITE); // watch file name changes and file write change

	if(change_handles[1] == INVALID_HANDLE_VALUE) {
		printf("FindFirstChangeNotification function failed (%d).\n", GetLastError());
		return;
	}

	scope_guard guard;
	guard += [&change_handles]() {
		::FindCloseChangeNotification(change_handles[1]);
	};
	
	bool running_state = true;
	while(running_state) {

		DWORD wait_status = ::WaitForMultipleObjects(_countof(change_handles), change_handles, FALSE, INFINITE);
		switch(wait_status) {
		case WAIT_OBJECT_0:
			running_state = false;
			break;
		case WAIT_OBJECT_0 + 1:
		{
			{
				std::lock_guard<std::mutex> lock(m_registrations_lock);
				//m_registrations.find()
				//m_registrations.notify_all()
			}
			//TODO: refresh_directory();
			if(!::FindNextChangeNotification(change_handles[1])) {
				printf("FindNextChangeNotification function failed (%d).\n", GetLastError());
				running_state = false;
			}
		}
			break;
		default:
			printf("Unhandled wait status (%d)\n", wait_status);
			break;
		}
	}
}

} // end of namespace watcher