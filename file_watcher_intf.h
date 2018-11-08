#pragma once

#include "registration_holder.h"
#include <string>

namespace watcher {

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
class on_file_changed_intf
{
public:
	/*! \brief Method triggered when some message is active/inactive state, it returns list of placement(s) only
		\param [in]	placementIdList	list of placement which they have some active message
	*/
	virtual void on_change(const std::wstring& file_path) = 0;
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class file_intf
{
public:
	virtual std::shared_ptr<registration::registrator_intf> subscribe(const std::wstring& file_path, on_file_changed_intf& event_handler) = 0;
};

} // end of namespace watcher