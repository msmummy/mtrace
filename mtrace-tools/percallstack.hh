#ifndef _PERCALLSTACK_HH_
#define _PERCALLSTACK_HH_

/**
 * A utility class to track information per call stack.  An instance
 * of T will be created for each new call stack and deleted when its
 * call stack terminates.  T's constructor will be passed the
 * mtrace_entry that created the call stack, plus the additional
 * arguments passed to handle.
 */
template<typename T>
class PerCallStack
{
public:
    template<typename... Args>
    void handle(const mtrace_fcall_entry* fcall, Args... args)
    {
        int cpu = fcall->h.cpu;

        switch (fcall->state) {
        case mtrace_start:
            if (call_stacks_.count(fcall->tag))
                die("PerCallStack::handle: cannot start call stack %"PRIx64"; already exists", fcall->tag);
            if (current_[cpu])
                die("PerCallStack::handle: cannot start call stack %"PRIx64"; cpu %d already has a call stack", fcall->tag, cpu);
            current_[cpu] = new T(fcall, args...);
            call_stacks_[fcall->tag] = current_[cpu];
            break;
        case mtrace_pause:
            if (!current_[cpu])
                die("PerCallStack::handle: cannot pause call stack %"PRIx64"; cpu %d has no call stack", fcall->tag, cpu);
            current_[cpu] = NULL;
            break;
        case mtrace_resume:
            if (!call_stacks_.count(fcall->tag))
                die("PerCallStack::handle: cannot resume call stack %"PRIx64"; unknown tag", fcall->tag);
            if (current_[cpu])
                die("PerCallStack::handle: cannot resume call stack %"PRIx64"; cpu %d already has a call stack", fcall->tag, cpu);
            current_[cpu] = call_stacks_.find(fcall->tag)->second;
            break;
        case mtrace_done:
            if (!current_[cpu])
                die("PerCallStack::handle: cannot end call stack %"PRIx64"; cpu %d has no call stack", fcall->tag, cpu);
            delete current_[cpu];
            current_[cpu] = NULL;
            call_stacks_.erase(fcall->tag);
            break;
        default:
            die("PerCallStack::handle: unknown fcall state %d", fcall->state);
        }
    }

    T* current(int cpu)
    {
        return current_[cpu];
    }

    /**
     * Terminate any remaining call stacks.  This is useful during an
     * exit handler, for example.
     */
    void flush()
    {
        for (T *&it : current_) {
            if (it)
                delete it;
            it = NULL;
        }
    }

private:
    T* current_[MAX_CPUS];
    map<uint64_t, T*> call_stacks_;
};

#endif