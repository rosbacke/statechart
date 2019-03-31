#include <iostream>
#include <chrono>
#include <fmt/format.h>

#include "StateChart.h"

#include <termios.h>
#include <unistd.h>
#include <fcntl.h>

using std::cout;
using std::endl;
using std::chrono::time_point;
using std::chrono::system_clock;
using std::chrono::milliseconds;
using std::chrono::seconds;
using std::chrono::minutes;
using std::chrono::hours;
using std::chrono::duration_cast;
using std::string;

enum class EventId
{
	no_key,
	tick,
	key,
	arrow_up,
	arrow_down,
	arrow_left,
	arrow_right
};

class Event
{
public:
	using Id = EventId;
	Event(Id id) : m_id(id), m_key(0) {}
	Event(int key) : m_id(Id::key), m_key(key) {}

	Id m_id;
	int m_key;
};

class NonBlockKeys
{
public:
	NonBlockKeys() : m_fd(fileno(stdin))
	{
		tcgetattr(m_fd, &m_of);
		auto nf = m_of;
		cfmakeraw(&nf);
		tcsetattr(m_fd, TCSANOW, &nf);
		fcntl(m_fd, F_SETFL, fcntl(m_fd, F_GETFL) | O_NONBLOCK);
	}
	~NonBlockKeys() {		
		fcntl(m_fd, F_SETFL, fcntl(m_fd, F_GETFL) & ~O_NONBLOCK);
		tcsetattr(m_fd, TCSANOW, &m_of);
	}
	int readChar()
	{
		char c = 0;
		int res = read(m_fd, &c, 1);
		if (res == -1) {
			auto e = errno;
			if (e == EAGAIN || e == EWOULDBLOCK) {
				return 0;
			}
		}
		if (res == 0)
			return 0;

		return c;
	}

	Event getChar()
	{
		auto t = readChar();
		if (t == 0)
			return EventId::no_key;
		if (t == 27 && readChar() == 91)
		{
			switch(readChar()) {
			case 65: return EventId::arrow_up;
			case 66: return EventId::arrow_down;
			case 67: return EventId::arrow_right;
			case 68: return EventId::arrow_left;
			default: return EventId::no_key;
			}
		}
		return Event{int{t}};
	}

private:
	int m_fd;
	struct termios m_of;
};

enum class StateId
{
	showTime,
	stateIdNo
};

class DigitalWatch;

struct StateDesc
{
	using Event = ::Event;
	using StateId = ::StateId;
	using Fsm = DigitalWatch;
	static void setupStates(FsmSetup<StateDesc>& sc);
};

class DigitalWatch : public FsmBase<StateDesc>
{
public:
	DigitalWatch();
	void print(string time, int charPos, bool show);

	using tp = system_clock::time_point;
	tp m_base;
};

class ShowTimeState : public StateBase<StateDesc, StateId::showTime>
{
public:
	explicit ShowTimeState(StateArgs& args) : StateBase(args) {}
	bool event(const Event& ev)
	{
		using Id = Event::Id;
		switch(ev.m_id)
		{
		case Id::tick:
			tick();
			break;
		case Id::arrow_left:
			m_offset -= m_offset > 0 ? 1 : 0;
			break;
		case Id::arrow_right:
			m_offset += m_offset < 10 ? 1 : 0;
			break;
		}
		return true;
	}

	void tick();
	int m_offset = 0;
};

void ShowTimeState::tick()
{
	auto delta = system_clock::now() - fsm().m_base;
	int millisec = duration_cast<milliseconds>(delta).count() % 1000;
	int sec = duration_cast<seconds>(delta).count() % 60;
	int min = duration_cast<minutes>(delta).count() % 60;
	int hour = duration_cast<hours>(delta).count() % 24;

	char c = ':'; //millisec >= 500 ? ' ' : ':';
	bool show = millisec >= 250 && millisec < 750;
	string time = fmt::format("{:02}{}{:02}{}{:02}", hour, c, min, c, sec);
	fsm().print(time, m_offset, show);
}

void StateDesc::setupStates(FsmSetup<StateDesc>& sc)
{
	sc.addState<ShowTimeState>();
}

DigitalWatch::DigitalWatch()
{
	m_base = system_clock::now();
	fmt::print("\n\n");
}

void DigitalWatch::print(string time, int charPos, bool showStr)
{
	int offset = charPos + (charPos < 3 ? 0 : charPos < 5 ? 1 : 2);
	if (showStr)
		time = string(" ") + time;
	else
		time = string(1 + time.size(), ' ');

	fmt::print("\r {}", time);
	fmt::print("\r {}", string(time.begin(), time.begin() + offset));	
	cout.flush();
}

int main()
{
	fmt::print("   Digital Watch   \n");
	fmt::print("Use arrow keys to control.\n");
	fmt::print("Press 'x' to quit.\n\n");

	NonBlockKeys nbKeys;
	DigitalWatch dw;
	dw.setStartState(StateId::showTime);
	using Id = Event::Id;

	while(1)
	{
		Event ev = nbKeys.getChar();
		// cout << int(ev.m_id) << " " << int(ev.m_key) << "\n\r";
		if (ev.m_id == Id::key && ev.m_key == 'x')
			break;

		if (ev.m_id == Id::no_key)
			dw.postEvent(Event{Id::tick});
		else
			dw.postEvent(ev);

		cout.flush();
		usleep(50000);
	}
	cout << "\r" << endl;
}
