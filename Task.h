#pragma once
#include <tuple>
#include <functional>

#define TaskBegin()                             \
    do {                                        \
        if (task._jmp)                          \
            goto *task._jmp;                    \
    } while (0)

#define _TaskYield()                            \
    do {                                        \
        __label__ jmp;                          \
        task._jmp = &&jmp;                      \
        return;                                 \
        jmp:;                                   \
    } while (0)

#define TaskYield()                             \
    do {                                        \
        task._state = Task::State::Wait;        \
        _TaskYield();                           \
        task._state = Task::State::Run;         \
        task._didWork = true;                   \
    } while (0)

#define TaskWait(cond)                          \
    do {                                        \
        task._state = Task::State::Wait;        \
        while (!(cond)) _TaskYield();           \
        task._state = Task::State::Run;         \
        task._didWork = true;                   \
    } while (0)

#define TaskSleepMs(ms)                         \
    do {                                        \
        task._state = Task::State::Wait;        \
        task._sleepStartMs = Task::TimeMs();    \
        task._sleepDurationMs = (ms);           \
        do _TaskYield();                        \
        while (!task._sleepDone());             \
        task._state = Task::State::Run;         \
        task._didWork = true;                   \
    } while (0)

#define TaskEnd()                               \
    do {                                        \
        __label__ jmp;                          \
        task._state = Task::State::Done;        \
        task._jmp = &&jmp;                      \
        jmp:;                                   \
        return;                                 \
    } while (0)

class IRQState {
public:
    // Functions provided by client
    static bool SetInterruptsEnabled(bool en);
    static void WaitForInterrupt();
    
    static IRQState Disabled() {
        IRQState irq;
        irq.disable();
        return irq;
    }
    
    IRQState()                  = default;
    IRQState(const IRQState& x) = delete;
    IRQState(IRQState&& x)      = default;
    
    ~IRQState() {
        restore();
    }
    
    void enable() {
        _Assert(!_prevEnValid);
        _prevEn = SetInterruptsEnabled(true);
        _prevEnValid = true;
    }
    
    void disable() {
        _Assert(!_prevEnValid);
        _prevEn = SetInterruptsEnabled(false);
        _prevEnValid = true;
    }
    
    void restore() {
        if (_prevEnValid) {
            SetInterruptsEnabled(_prevEn);
            _prevEnValid = false;
        }
    }
    
private:
    static void _Assert(bool cond) { if (!cond) abort(); }
    
    bool _prevEn = false;
    bool _prevEnValid = false;
};

class Task {
public:
    // Functions provided by client
    static uint32_t TimeMs();
    
    enum class State {
        Run,
        Wait,
        Done,
    };
    
    using TaskFn = std::function<void(Task& task)>;
    
    template <typename ...Tasks>
    static void Run(Tasks&... ts) {
        const std::reference_wrapper<Task> tasks[] = { static_cast<Task&>(ts)... };
        for (;;) {
            IRQState irq = IRQState::Disabled();
            // Execute every task
            bool didWork = false;
            for (Task& task : tasks) {
                didWork |= task.run();
            }
            // If no task performed work, go to sleep
            if (!didWork) IRQState::WaitForInterrupt();
        }
    }
    
    Task(TaskFn fn) : _fn(fn) {}
    
    void reset() {
        _state = State::Run;
        _jmp = nullptr;
    }
    
    bool run() {
        _didWork = false;
        switch (_state) {
        case State::Run:
        case State::Wait:
            _fn(*this);
            break;
        default:
            break;
        }
        return _didWork;
    }
    
    bool _sleepDone() const {
        return (TimeMs()-_sleepStartMs) >= _sleepDurationMs;
    }
    
    TaskFn _fn;
    State _state = State::Run;
    bool _didWork = false;
    void* _jmp = nullptr;
    uint32_t _sleepStartMs = 0;
    uint32_t _sleepDurationMs = 0;
};

template <typename T, size_t N>
class Channel {
public:
    class ReadResult {
    public:
        ReadResult() {}
        ReadResult(const T& x) : _x(x), _e(true) {}
        constexpr operator bool() const { return _e; }
        constexpr const T& operator*() const& { return _x; }
    
    private:
        T _x;
        bool _e = false;
    };
    
    bool readable() const {
        IRQState irq = IRQState::Disabled();
        return _readable();
    }
    
    bool writeable() const {
        IRQState irq = IRQState::Disabled();
        return _writeable();
    }
    
    T read() {
        IRQState irq = IRQState::Disabled();
        _Assert(_readable());
        return _read();
    }
    
    void write(const T& x) {
        IRQState irq = IRQState::Disabled();
        _Assert(_writeable());
        _write(x);
    }
    
    ReadResult readTry() {
        IRQState irq = IRQState::Disabled();
        if (!_readable()) return {};
        return _read();
    }
    
    bool writeTry(const T& x) {
        IRQState irq = IRQState::Disabled();
        if (!_writeable()) return false;
        _write(x);
        return true;
    }
    
    void reset() {
        _rptr = 0;
        _wptr = 0;
        _full = 0;
    }
    
private:
    static void _Assert(bool cond) { if (!cond) abort(); }
    
    bool _readable() const  { return (_rptr!=_wptr || _full);   }
    bool _writeable() const { return !_full;                    }
    
    T _read() {
        T r = _buf[_rptr];
        _rptr++;
        // Wrap _rptr to 0
        if (_rptr == N) _rptr = 0;
        _full = false;
        return r;
    }
    
    void _write(const T& x) {
        _buf[_wptr] = x;
        _wptr++;
        // Wrap _wptr to 0
        if (_wptr == N) _wptr = 0;
        // Update `_full`
        _full = (_rptr == _wptr);
    }
    
    T _buf[N];
    size_t _rptr = 0;
    size_t _wptr = 0;
    bool _full = false;
};
