#ifndef _THREAD_H
#define _THREAD_H

#include "uthreads.h"
#include <queue>

#define MAIN_ID 0

enum State {
    READY, RUNNING, BLOCKED, SLEEP
};

class Thread {
private:
    int _id;
    State _state;
    char *_stack;
    int _quantums = 0;
    int _sleep_duration = 0;
    bool _is_sleeping = false;
public:
    static int count;

    Thread(int id, State state) : _id(id), _state(state) {
        if (id != MAIN_ID) {
            _stack = new char[STACK_SIZE];
        }
        ++count;
    }

    ~Thread() {
        --count;
        delete[] _stack;
    }

    int get_id() const {
        return _id;
    }

    char *get_sp() const {
        return _stack;
    }

    int get_quantums() const {
        return _quantums;
    }

    State get_state() const {
      return _state;
    }

    void set_state(State new_state) {
        _state = new_state;
    }

    void increase_quantums() {
        ++_quantums;
    }

    int get_is_sleeping() const {
      return _is_sleeping;
    }

    void set_is_sleeping(bool is_sleeping)  {
      _is_sleeping = is_sleeping;
    }

    int get_sleep_duration() const {
      return _sleep_duration;
    }

    void set_sleep_duration(int duration) {
      _sleep_duration = duration;
    }

    void decrease_sleep_duration() {
      _sleep_duration--;
    }

};


#endif
