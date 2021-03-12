#include <cassert>
#include "StateMachine.h"
 
StateMachine::StateMachine(unsigned char maxStates) :
    _maxStates(maxStates),
    _eventGenerated(false)
{
}    
 
// generates an external event. called once per external event 
// to start the state machine executing
void StateMachine::ExternalEvent(unsigned char newState)
{
    // if we are not supposed to ignore this event
    if (newState != EVENT_IGNORED) {
        // generate the event and execute the state engine
        InternalEvent(newState); 
        StateEngine();     
    }
}

// generates an internal event. called from within a state 
// function to transition to a new state
void StateMachine::InternalEvent(unsigned char newState)
{
    _eventGenerated = true;
    currentState = newState;
}

// the state engine executes the state machine states
void StateMachine::StateEngine(void)
{
		mtx.lock();
    // while events are being generated keep executing states
    while (_eventGenerated) {         
        _eventGenerated = false;  // event used up, reset flag
 
        assert(currentState < _maxStates);

		    // get state map
        const StateStruct* pStateMap = GetStateMap();

        // execute the state
        (this->*pStateMap[currentState].pStateFunc)();
    }
		mtx.unlock();
}
