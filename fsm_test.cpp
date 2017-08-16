/*
 * PosixFileIfReal_test.cpp
 *
 *  Created on: 30 okt. 2016
 *      Author: mikaelr
 */

// #include "hal/PosixFileReal.h"
#include "StateChart.h"

#include <gtest/gtest.h>

#include <string>
#include <iostream>

using std::cout;
using std::endl;

/**
 * Each state machine needs some event class. Define a new class
 * 'TestEvent' to use in our FSM.
 */
class TestEvent
{
  public:
    /**
     * Use enum values as identifiers. This is recommended but optional.
     * During event delivery, the events will be enqueued on a local queue
     * and needs to be copyable.
     */
    enum class TestEventId
    {
        testEvent1,
        testEvent2,
        testEvent3,
    };

    explicit TestEvent(TestEventId id) : m_id(id)
    {
    }
    TestEventId m_id;
};

// Name of our FSM. Forward declared.
class MyTestFsm;

// A description class tying together the types needed for our FSM.
class TestFsmDescription
{
  public:
    // Each state is represented by a class and a an enum value.
    // This enum needs exactly one value for each state class.
    enum class StateId
    {
        state1,
        state2,
        state3,
    };

    // Needed helper function. Convert each state class into a string
    // for e.g. logging.
    static std::string toString(StateId id);

    // Typename 'Event' indicate class name for rhe event class to use.
    using Event = TestEvent;

    // Typename 'Fsm' Indicate the class that implement our FSM.
    // External code will deliver events to this class using postEvent fkn.
    using Fsm = MyTestFsm;
};

// Our FSM class. It needs to inherit from 'FsmBase'.
// FsmBase has the 'postEvent function.
class MyTestFsm : public FsmBase<TestFsmDescription>
{
  public:
    MyTestFsm();

    int testD2 = -2;
    int testD3 = -3;

    int myUserFsmData = 0;
};

// Helper 'using' to save some typing.
using StateId = TestFsmDescription::StateId;
using EventId = TestEvent::TestEventId;

static int testData = -1;

// First user state. Each state needs to inherit from StateBase,
// supplying the description class and state enum value
// for this particular state.
class TestState1 : public StateBase<TestFsmDescription, StateId::state1>
{
  public:
    // Constructor. Called when the state is entered.
    explicit TestState1(StateArgs& args) : StateBase(args)
    {
        testData = 0;
        cout << "State1, entry" << endl;
    }

    // Destructor, called when the state is left.
    ~TestState1()
    {
        testData = 10;
        cout << "State1, exit" << endl;
    }

    // Event delivery function. Each state needs to implement this.
    // should return 'true' if the event was handled and no more
    // base state is to be called. Return true and the base states will
    // see the event.
    bool event(const TestEvent& ev)
    {
        cout << "State1, event : " << int(ev.m_id) << endl;
        testData = 1;
        if (ev.m_id == EventId::testEvent1)
        {
            transition(StateId::state2);
        }
        if (ev.m_id == EventId::testEvent3)
        {
            transition(StateId::state3);
        }
        return false;
    }
};

// Helper base class to add common functionality. (and reduce typing.)
// Useful for these tests but not needed i general.
// However, each state class needs to to inherit (transitively) from
// 'StateBase'.
template <StateId id>
class MyStateBase : public StateBase<TestFsmDescription, id>
{
  public:
    // Introduce the Fsm type from base. Could use 'TestFSM' also.
    // auto solves this now so commented out.
    // using typename StateBase<TestFsmDescription, id>::Fsm;

    // Each state will receive an argument pack when the state is entered
    // (constructed)
    // This needs to be passed on to the base class.
    MyStateBase(StateArgs& args) : StateBase<TestFsmDescription, id>(args)
    {
    }

    // Common support functions for states can be set up here.
    int getFsmData()
    {
        // StateBase offers 'fsm()' to give access to the users FSM class.
        auto& myFsm = StateBase<TestFsmDescription, id>::fsm();
        return myFsm.myUserFsmData;
    }
};

// Second user state class.
class TestState2 : public MyStateBase<StateId::state2>
{
  public:
    explicit TestState2(StateArgs args) : MyStateBase(args)
    {
        testData = 5;
        cout << "State2, entry" << endl;
    }

    ~TestState2()
    {
        testData = 11;
        cout << "State2, exit" << endl;
    }

    bool event(const TestEvent& ev)
    {
        cout << "State2, event : " << int(ev.m_id) << endl;

        if (ev.m_id == EventId::testEvent1)
        {
            transition(StateId::state1);
            testData = 8;
        }
        if (ev.m_id == EventId::testEvent2)
        {
            testData = 15;
            fsm().testD2 = 2;
            return false;
        }
        if (ev.m_id == EventId::testEvent3)
        {
            transition(StateId::state3);
        }
        testData = 9;
        return false;
    }
};

class TestState3 : public MyStateBase<StateId::state3>
{
  public:
    explicit TestState3(StateArgs args) : MyStateBase(args)
    {
        testData = 15;
        cout << "State3, entry" << endl;
    }

    ~TestState3()
    {
        testData = 111;
        cout << "State3, exit" << endl;
    }

    bool event(const TestEvent& ev)
    {
        cout << "State3, event : " << int(ev.m_id) << endl;

        if (ev.m_id == EventId::testEvent1)
        {
            transition(StateId::state1);
            testData = 18;
        }
        if (ev.m_id == EventId::testEvent2)
        {
            testData = 115;
            fsm().testD3 = 3;
            return false;
        }
        testData = 19;
        return false;
    }
};

// Implementation of the state 'toString'.
// If not needed, supply a minimal dummy implementation.
std::string
TestFsmDescription::toString(StateId id)
{
    switch (id)
    {
    case TestFsmDescription::StateId::state1:
        return "state1";
    case TestFsmDescription::StateId::state2:
        return "state2";
    case TestFsmDescription::StateId::state3:
        return "state3";
    }
    return "";
}

// Constructor of our state machine. It is responsible for setting
// up the state hierarchy. Each state needs to have one 'addState'.
MyTestFsm::MyTestFsm()
{
    // Add a state without a parent state. Indicates that the state
    // is a base level state.
    addState<TestState1>();
    addState<TestState2>();

    // Add a state with a parent state. Each time this is active the
    // parent state will be entered first. No particular depth
    // limitation exist so states can have parents recursively.
    addState<TestState3, StateId::state1>();
}

TEST(StateChart, testStateChart)
{
    cout << "start" << endl;

    // Construct the FSM. Will set up the state tree.
    MyTestFsm myFsm;
    cout << "do 2" << endl;

    EXPECT_EQ(-2, myFsm.testD2);

    TestEvent ev1(EventId::testEvent1);
    TestEvent ev2(EventId::testEvent2);
    TestEvent ev3(EventId::testEvent3);

    cout << "do 3" << endl;
    EXPECT_EQ(-1, testData);

    // Each FSM needs to be started. This will enter the given state
    // and run constructions accordingly.
    myFsm.setStartState(StateId::state1);

    EXPECT_EQ(0, testData);
    cout << "do 4" << endl;

    // Pos an event to the state machine. It will be delivered to the current
    // active
    // state 'event' function and possibly to it's parent states.
    myFsm.postEvent(ev2);
    EXPECT_EQ(1, testData);
    cout << "do 5" << endl;

    myFsm.postEvent(ev1); // Pass over to state2.
    EXPECT_EQ(5, testData);
    EXPECT_EQ(-2, myFsm.testD2);
    cout << "do 6" << endl;

    myFsm.postEvent(ev2);
    EXPECT_EQ(15, testData);
    EXPECT_EQ(2, myFsm.testD2);
    cout << "do 7" << endl;

    myFsm.postEvent(ev1);
    EXPECT_EQ(0, testData);
    EXPECT_EQ(2, myFsm.testD2);
    cout << "do 8" << endl;

    myFsm.postEvent(ev3); // Pass over to state3.

    // EXPECT_EQ(3, myFsm.testD3);
    // From state 3 constructor.
    EXPECT_EQ(15, testData);

    myFsm.postEvent(ev2); // Check parent.

    // When the Fsm is destructed, all current active states are destructed.
}

int
main(int ac, char* av[])
{
    testing::InitGoogleTest(&ac, av);
    return RUN_ALL_TESTS();
}