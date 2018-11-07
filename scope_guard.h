#pragma once

#include <functional>
#include <deque>
#include <assert.h>

class scope_guard {
public:
	enum execution { always, when_return, when_exception };

	scope_guard(scope_guard &&) = default;
	explicit scope_guard(execution policy = always) : m_policy(policy) {}

	template<class Callable>
	scope_guard(Callable&& func, execution policy = always) : m_policy(policy) {
		this->operator += <Callable>(std::forward<Callable>(func));
	}

	template<class Callable>
	scope_guard& operator += (Callable&& func) try {
		m_handlers.emplace_front(std::forward<Callable>(func));
		return *this;
	} catch(...) {
		if(m_policy != when_return) func();
		throw;
	}

	~scope_guard() {
		if(m_policy == always || ((m_policy == when_exception) == std::uncaught_exception())) {
			for(auto& f: m_handlers) try {
				f(); // must not throw
			} catch(...) { 
				assert(false && "Scope guard function throw exception!!!, this is not allowed");
			}
		}
	}

	void dismiss() noexcept {
		m_handlers.clear();
	}

private:
	scope_guard(const scope_guard&) = delete;
	void operator = (const scope_guard&) = delete;

	std::deque<std::function<void()>> m_handlers;
	execution m_policy = always;
};