#pragma once

#include <memory>
#include <list>
#include <functional>
#include <mutex>
#include <vector>

#include "registrator_intf.h"

namespace ini
{

/*
	RegistrationToken class keeps a callback registered in a RegistrationHolder instance registered until you either destroy
	this object or call the Unregister method manually. Registrations are reference-counted, therefore you can copy tokens
	around and the registration is valid as long as at least one token is alive.
*/

class registration_token : public registrator_intf
{
public:
	registration_token() : m_notify_mutex(nullptr) {};

	registration_token(std::shared_ptr<void> registration_smptr, std::recursive_mutex * notify_mutex)
		: m_registration_holder(std::move(registration_smptr)), m_notify_mutex(notify_mutex)
	{}

	~registration_token()
	{
		unregister();
	}

	void unregister() override
	{
		std::unique_lock<std::recursive_mutex> notifylock;
		if (m_notify_mutex) {
			// Check there is no concurrent notification in progress, otherwise we may destroy this callback while some other threads executes it.
			notifylock = std::unique_lock<std::recursive_mutex>{ *m_notify_mutex };
		}

		m_registration_holder.reset();
		m_notify_mutex = nullptr;
	}

private:
	std::shared_ptr<void> m_registration_holder;
	std::recursive_mutex* m_notify_mutex;
};

/*
	RegistrationHolder class implements holder for callback registrations. You can register arbitrary functors as callbacks:

	RegistrationHolder<> regholder;
	int a = 0;
	RegistrationToken token1 = regholder.Register([&a] {
		++a;
	});
	RegistrationToken token2 = regholder.Register([] {
		dispatcher.sendmsg("it happened!");
	});

	and after some time you can notify all registered handlers:

	regholder.NotifyAll();

	which calls all registered callbacks consecutively. If you wish to unregister you callback, simply destroy the token you recieved from Register() or call token's Unregister method:

	token1.Unregister();

	after returning from Unregister() you are guaranteed the callback wont be called any more. If any callback throws an exception, any other callback won't be called and the exception will propagate from NotifyAll().
	Your callbacks may also have parameters (how many you wish), in that case use template arguments to the RegistrationHolder class:

	enum EventType {
		msg_arrived,
		connection_closed
	};
	RegistrationHolder<EventType> regholder;
	auto token = regholder.Register([] (EventType type) {
		if (type == msg_arrived)
			do_stuff();
		else
			do_some_other_stuff();
	});
	...
	regholder.NotifyAll(msg_arrived);

	If you need to perform some action when your callback is being unregistered, use the optional 'unregister handler' parameter:

	auto token = regholder.Register(
		[] {
			... callback body ...
		},
		[] {
			notify_somebody_i_am_no_more();
		}
	);

	All operations over RegistrationHolder are thread-safe and callback are never executed concurrently.
	You can also register and unregister callbacks from callback body. Usually you want to do this when you need a callback to unregister itself:

	RegistrationHolder<MyClass &> regholder;
	MyClass myclass;
	myclass.token = regholder.Register([&myclass](int event_type){
		if (event_type == 123)
			myclass.token.Unregister();
	});

	NOTE: There is one peculiarity in unregistering callback from itself - in the standard case, the callback object is destroyed right before
	returning from Unregister(). If a callback unregisters itself, the callback object lives until its last execution finishes.

	--------------------------------------------------------------------------
	HOW TO REGISTER CBK FUNCTION WITH RETURN TYPE:

	reg_holder<bool, int> regholder;
	class MyClass {
		bool on_event(int type) {
			if (type == 0) {
				return false;
			}
			return true;
		}
	}
	MyClass myclass;
	myclass.token = regholder.Register([&myclass](int type){
		myclass.on_event(type);
	});

	...
	...

	std::list<bool> regholder.notify_all(UI_OPEN);

*/

template <typename TReturnType, typename ... CallbackArguments>
class reg_holder
{
public:
	std::shared_ptr<registrator_intf> Register(std::function<TReturnType(CallbackArguments...)> callback)
	{
		return register_impl(std::move(callback), std::function<void()>());
	}

	std::shared_ptr<registrator_intf> Register(std::function<TReturnType(CallbackArguments...)> callback, std::function<void()> endhandler)
	{
		return register_impl(std::move(callback), std::move(endhandler));
	}
	
	template <typename ... Args, typename = std::enable_if_t<std::is_void<TReturnType>::value>>
	void notify_all(Args &&... args)
	{
		notify_all_impl(std::forward<Args>(args)...);
	}
	
	template<typename ... Args, typename = std::enable_if_t<!std::is_void<TReturnType>::value>>
	std::list<TReturnType> notify_all(Args &&... args)
	{
		return notify_all_impl2(std::forward<Args>(args)...);
	}

    bool isEmpty() 
	{
		std::lock_guard<std::mutex> regslock(m_registrations_lock);
		return m_registrations.empty();
	}
    
	reg_holder() = default;
	reg_holder(reg_holder &&) = default;
	reg_holder & operator=(reg_holder &&) = default;
	
private:

	struct registration_entry
	{
		registration_entry(std::function<TReturnType(CallbackArguments...)> && cb, std::function<void()> && endhandler)
			: m_callback(std::move(cb)), m_endhandler(std::move(endhandler))
		{}

		registration_entry(registration_entry &&) = default;

		std::function<TReturnType(CallbackArguments...)> m_callback;
		std::function<void()> m_endhandler;
	};

	std::shared_ptr<registration_token> register_impl(std::function<TReturnType(CallbackArguments...)> && callback, std::function<void()> && endhandler)
	{
		std::unique_lock<std::mutex> regslock(m_registrations_lock);

		regslock = collect_unregistered(std::move(regslock));

		auto p = std::shared_ptr<registration_entry>(
			new registration_entry(std::move(callback), std::move(endhandler)),
			[](registration_entry * p) {
			if (p->m_endhandler) {
				p->m_endhandler();
			}
			delete p;
		}
		);

		m_registrations.emplace_back(p);
		return std::make_shared<registration_token>(std::move(p), &m_notification_lock);
	}

	template <typename ... Args, typename = std::enable_if<std::is_void<TReturnType>::value>>
	void notify_all_impl(Args &&... args)
	{
		std::vector<std::weak_ptr<registration_entry>> entries;

		{
			std::lock_guard<std::mutex> regslock(m_registrations_lock);
			entries.insert(entries.end(), m_registrations.begin(), m_registrations.end());
		}

		{
			std::lock_guard<std::recursive_mutex> notificationslock(m_notification_lock);

			for (const std::weak_ptr<registration_entry> & p : entries)
			{
				std::shared_ptr<registration_entry> shrp = p.lock();
				if (shrp) {
					shrp->m_callback(std::forward<Args>(args)...);
				}
			}
		}
	}

	template<typename ... Args, typename = std::enable_if_t<!std::is_void<TReturnType>::value>>
	std::list<TReturnType> notify_all_impl2(Args &&... args)
	{
		std::list<TReturnType> resultList;

		std::vector<std::weak_ptr<registration_entry>> entries;

		{
			std::lock_guard<std::mutex> regslock(m_registrations_lock);
			entries.insert(entries.end(), m_registrations.begin(), m_registrations.end());
		}

		{
			std::lock_guard<std::recursive_mutex> notificationslock(m_notification_lock);

			for (const std::weak_ptr<registration_entry> & p : entries)
			{
				std::shared_ptr<registration_entry> shrp = p.lock();
				if (shrp) {
					TReturnType res = shrp->m_callback(std::forward<Args>(args)...);
					resultList.push_back(std::move(res));
				}
			}
		}

		return resultList;
	}

	std::unique_lock<std::mutex> collect_unregistered(std::unique_lock<std::mutex> regslock)
	{
		m_registrations.remove_if([](const std::weak_ptr<registration_entry> & e) {
			return e.expired();
		});

		return regslock;
	}

	std::list<std::weak_ptr<registration_entry>> m_registrations;

	std::mutex m_registrations_lock;
	std::recursive_mutex m_notification_lock;
};


template <typename ... CallbackArguments>
using registration_holder = reg_holder<void, CallbackArguments...>;

} // end of namespace ini
