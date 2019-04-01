#include <iostream>
#include <chrono>
#include <fmt/format.h>

#include "StateChart.h"

#include <termios.h>
#include <unistd.h>
#include <fcntl.h>

#include <date/date.h>

using std::cout;
using std::endl;
using namespace std::chrono;
using namespace date;
using std::string;

/**
Design idea:
Have a mode (Abbr):
- (fa) Initial, no time set. (Go to Set time)
- (ti) Show time.
- (da) Show date.
- (al) Show alarm.
- (sw) Stopwatch.
- (mt) Middle time.
- (st) Set time.
- (sd) Set date.
- (sa) Set alarm.

Some mode auto ends after short time.
- Show date. -> Show time.
- Show alarm. -> Show time.

When cursor is at left side, left arrow quits application.
Visuals:
- 2 characters indicating mode to the left.
- During set operations, all numbers blink.
- During show time, colon blinks slowly.
- During Middle time + running stopwatch, colon blinks.
- During non-set opeations, cursor is at line start.

- During set operations, cursor at end column and key right commit the set.

- When cursor is at line start, up/down changes mode.
- When cursor not at line start: up/down changes numbers.

- During sw: Right arrow toggles running.
- During sw and running: Left arrow toggles middle time.
- During sw and stopped: Left arrow reset stopwatch time.
*/

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

enum class StateId;

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

class LClock
{
public:
	using Duration = std::chrono::system_clock::duration;
	using TimePoint = std::chrono::system_clock::time_point;

	LClock() = default;
	TimePoint now() const
	{
		return std::chrono::system_clock::now() + m_diff;
	}

	void setTime(Duration timeOfDay)
	{
		auto now = system_clock::now();
		auto localTime = now + m_diff;
		auto today = floor<days>(localTime);
		m_diff = (today + timeOfDay) - now;
	}

	void setDate(sys_days days_)
	{
		auto now = system_clock::now();
		auto localTime = now + m_diff;
		auto timeOfDay = localTime - floor<days>(localTime);
		m_diff = (timeOfDay + days_) - now;
	}
	Duration secSinceEpoch() const
	{
		return now().time_since_epoch();
	}
	sys_days daysSinceEpoch() const
	{
		return floor<days>(now());
	}

	int msec() const { return duration_cast<milliseconds>(secSinceEpoch()).count() % 1000; };
	int sec() const { return duration_cast<seconds>(secSinceEpoch()).count() % 60; };
	int min() const { return duration_cast<minutes>(secSinceEpoch()).count() % 60; };
	int hour() const { return duration_cast<hours>(secSinceEpoch()).count() % 24; };
	int day() const { return static_cast<unsigned>( year_month_day(daysSinceEpoch()).day()); }
	int month() const { return static_cast<unsigned>( year_month_day(daysSinceEpoch()).month()); }
	int year() const { return static_cast<int>( year_month_day(daysSinceEpoch()).year()); }

private:
	Duration m_diff = {};
};


class Display
{
public:
	Display(LClock& lc) : m_clock(lc) {}
	static const constexpr int o2Do[] = { 0, 4, 5, 7, 8, 10, 11 };
	static const constexpr int maxOffset = sizeof o2Do / sizeof(int);

	void update();
	void printTime();
	void print(int grp1, int grp2, int grp3, bool showColon);
	void print(string time, int charPos, bool showStr);

	bool cursorRight()
	{
		bool update = m_offset < maxOffset - 1;
		m_offset += update ? 1 : 0;
		return update;
	}
	bool cursorLeft()
	{
		bool update = m_offset > 0;
		m_offset -= update ? 1 : 0;
		return update;
	}
	void setMode(string m)
	{
		m_mode = m;
	}
	int m_offset = 0;
	bool m_blink = false;
	bool m_colonBlink = false;
	std::string m_mode;
	LClock& m_clock;
};

const constexpr int Display::o2Do[];

void Display::print(string time, int charPos, bool showStr)
{

	int dOffset = m_offset < maxOffset ? o2Do [m_offset] : 11;
	if (!showStr)
		time = string(time.size(), ' ');

	fmt::print("\r{}", time);
	fmt::print("\r{}", string(time.begin(), time.begin() + dOffset));
	cout.flush();
}

void Display::print(int grp1, int grp2, int grp3, bool showColon)
{
	char c = showColon ? ':' : ' ';
	string time = fmt::format(" {:2} {:02}{}{:02}{}{:02}", m_mode, grp1, c, grp2, c, grp3);
	print(time, m_offset, true);
}

void Display::printTime()
{
	int millisec = m_clock.msec();
	int sec = m_clock.sec();
	int min = m_clock.min();
	int hour = m_clock.hour();

	char c = ':';
	bool show = millisec >= 250 && millisec < 750;
	string time = fmt::format(" {:2} {:02}{}{:02}{}{:02}", m_mode, hour, c, min, c, sec);
	print(time, m_offset, true);
}

enum class StateId
{
	root,
	showTime,
	setTime,
	endState,
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
	void tick()
	{
		m_display.setMode(modeString(currentStateId()));
		m_display.printTime();
	}

	// Return a 2 letter indicator of the current mode.
	const char* modeString(StateId is) const;

	Display m_display;
	LClock m_clock;
};

DigitalWatch::DigitalWatch()
: m_display(m_clock)
{
}

const char* DigitalWatch::modeString(StateId is) const
{
	using SId = StateId;
	switch(is)
	{
	case SId::endState: return "en";
	case SId::setTime: return "st";
	case SId::showTime: return "ti";
	default: return "un";
	}
}


template<StateId id>
class State : public StateBase<StateDesc, id>
{
public:
	using EId = Event::Id;
	using SId = StateId;
	explicit State(StateArgs& args) : StateBase<StateDesc, id>(args) {}
};

class EndState : public State<StateId::endState>
{
public:
	explicit EndState(StateArgs& args) : State(args) {}
	bool event(const Event& ev)
	{ return false; }
};

class RootState : public State<StateId::root>
{
public:
	explicit RootState(StateArgs& args)
	: State(args), m_display(fsm().m_display) {}

	bool event(const Event& ev)
	{
		switch(ev.m_id)
		{
		case EId::arrow_left:
			if (m_display.m_offset == 0) {
				transition(StateId::endState);
			}
			break;
		case EId::key:
			if (ev.m_key == 'x') {
				transition(StateId::endState);
			}
			break;
		}
		return false;
	}
	Display& m_display;
};

class SetTimeState : public State<StateId::setTime>
{
public:
	explicit SetTimeState(StateArgs& args)
	: State(args), m_display(fsm().m_display)
	{
		m_display.setMode(fsm().modeString(SId::setTime));
		auto& clk = fsm().m_clock;
		m_sec = clk.sec();
		m_min = clk.min();
		m_hour = clk.hour();
	}
	~SetTimeState()
	{}
	bool event(const Event& ev)
	{
		switch(ev.m_id)
		{
		case EId::tick:
			m_display.print(m_hour, m_min, m_sec, true);
			break;
		case EId::arrow_left:
			m_display.cursorLeft();
			return true;
		case EId::arrow_right:
			if (!m_display.cursorRight()) {
				m_display.m_offset = 0;
				auto dt = hours(m_hour) + minutes(m_min) + seconds(m_sec);
				fsm().m_clock.setTime(dt);
				transition(SId::showTime);
			}
			return true;
		case EId::arrow_up:
		{
			auto add = [](int& val, int mult, int max)
			{
				if (val + mult < max)
					val += mult;
			};
			switch(m_display.m_offset)
			{
			case 0: transition(SId::showTime); return true;
			case 1: add(m_hour, 10, 24); return true;
			case 2: add(m_hour, 1, 24); return true;
			case 3: add(m_min, 10, 60); return true;
			case 4: add(m_min, 1, 60); return true;
			case 5: add(m_sec, 10, 60); return true;
			case 6: add(m_sec, 1, 60); return true;
			}
			break;
		}
		case EId::arrow_down:
		{
			auto sub = [](int& val, int mult)
			{
				if (val - mult >= 0)
					val -= mult;
			};
			switch(m_display.m_offset)
			{
			case 0: transition(SId::showTime); return true;
			case 1: sub(m_hour, 10); return true;
			case 2: sub(m_hour, 1); return true;
			case 3: sub(m_min, 10); return true;
			case 4: sub(m_min, 1); return true;
			case 5: sub(m_sec, 10); return true;
			case 6: sub(m_sec, 1); return true;
			}
			break;
		}
		}
		return false;
	}

	int m_sec;
	int m_min;
	int m_hour;
	Display& m_display;
};

class ShowTimeState : public State<StateId::showTime>
{
public:
	explicit ShowTimeState(StateArgs& args)
	: State(args), m_display(fsm().m_display)
	{
		m_display.setMode(fsm().modeString(SId::showTime));
	}
	bool event(const Event& ev);
	Display& m_display;
};

bool ShowTimeState::event(const Event& ev)
{
	switch(ev.m_id)
	{
	case EId::tick:
		m_display.printTime();
		fsm().tick();
		break;
	case EId::arrow_up:
		transition(StateId::setTime);
		break;
	case EId::arrow_down:
		transition(StateId::setTime);
		break;
	}
	return false;
}

void StateDesc::setupStates(FsmSetup<StateDesc>& sc)
{
	sc.addState<RootState>();
	sc.addState<ShowTimeState, RootState>();
	sc.addState<SetTimeState, RootState>();
	sc.addState<EndState>();
}

int main()
{
	fmt::print("   Digital Watch   \n");
	fmt::print("Use arrow keys to control.\n");
	fmt::print(" ti: Display current time.\n");
	fmt::print(" st: Set time.\n\n");
	fmt::print("Left arrow in ti to quit.\n\n");

	NonBlockKeys nbKeys;
	DigitalWatch dw;
	dw.setStartState(StateId::showTime);
	using EId = Event::Id;

	while(dw.currentStateId() != StateId::endState)
	{
		Event ev = nbKeys.getChar();
		if (ev.m_id != EId::no_key)
			dw.postEvent(ev);

		dw.postEvent(Event{EId::tick});
		cout.flush();
		usleep(50000);
	}
	cout << "\r" << endl;
}
