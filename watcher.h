#pragma once

#include <stdint.h>
#include <functional>
#include <map>

namespace ini {


using change_fn = std::function<void(const wchar_t* section, const wchar_t* value_name)>;

class watcher
{
public:
	void subscribe(const wchar_t* section, const wchar_t* value_name, change_fn&& fn);
	void unsubscribe(const wchar_t* section, const wchar_t* value_name);

private:
	struct value_subs {
		uint32_t  crc32 = 0;
		change_fn fn = nullptr;
	};
	// value map is represent by value name and crc32 of value
	using value_map = std::map<std::wstring, value_subs>;
	// held all subscriptions
	std::map<std::wstring, value_map> m_subscriptions;
};


} // end of namespace ini