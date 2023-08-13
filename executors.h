#include <memory>
#include <chrono>
#include <vector>
#include <functional>
#include <thread>
#include <condition_variable>
#include <deque>
#include <queue>
#include <atomic>
#include <iostream>

struct TaskError : public std::runtime_error
{
    using std::runtime_error::runtime_error;
};

#define TASK_ERROR_IF(expr, message) \
    if (expr) throw TaskError(message)

struct Cmp;
class Executor;
class TimerQueue;

void TimerWorker(TimerQueue* timer);
void Worker(size_t worker_id, Executor* executor);

enum class TaskStatus
{
    None       = 0,
    Unassigned = 1,
    Submited   = 2,
    Processing = 3,
    Canceled   = 4,
    Failed     = 5,
    Completed  = 6
};
using TimePoint = std::chrono::system_clock::time_point;

class Task : public std::enable_shared_from_this<Task>
{
public:
    virtual ~Task() {}

    virtual void Run() = 0;

    void AddDependency(std::shared_ptr<Task> dep);
    void AddTrigger(std::shared_ptr<Task> dep);
    void SetTimeTrigger(TimePoint at);

    bool               IsReadyToExecute();
    bool               IsCompleted();
    bool               IsFailed();
    bool               IsCanceled();
    bool               IsFinished();
    bool               HasTimeTrigger();
    std::exception_ptr GetError();

    void Cancel();
    void Wait();

private:
    friend void Worker(size_t worker_id, Executor* executor);
    friend void TimerWorker(TimerQueue* timer);
    friend class Executor;
    friend class TimerQueue;
    friend struct Cmp;

    void SubmitTo(Executor* executor);
    void ResetExecutor();
    void BeginProcessing();
    void Finish(TaskStatus status, std::exception_ptr ptr = nullptr);
    void UpdateReadyStatus();

    //*Result
    std::atomic<TaskStatus> status_   = {TaskStatus::Unassigned};
    std::atomic<Executor*>  executor_ = {nullptr};
    std::exception_ptr      e_ptr_    = nullptr;
    bool                    finished_ = false;

    //*Wait
    std::mutex              mutex_;
    std::condition_variable cv_;

    //*BeforeSubmit
    std::vector<std::weak_ptr<Task>>   sub_dependency_;
    std::vector<std::shared_ptr<Task>> sub_trigger_;
    TimePoint                          execute_after_;
    size_t                             dependency_count_ = 0;
    bool                               has_dependency_   = false;
    bool                               has_trigger_      = false;
    bool                               has_timer_        = false;
    bool                               no_condition_     = true;

    //*AfterSubmit
    std::atomic<size_t> finished_dependency_count_ = {0};
    std::atomic<bool>   triggered_                 = {false};
    std::atomic<bool>   the_time_has_come_         = {false};
};

template <class T> class Future;

template <class T> using FuturePtr = std::shared_ptr<Future<T>>;

// Used instead of void in generic code
struct Unit
{};

enum class ExecutorState
{
    None     = 0,
    Working  = 1,
    ShutDown = 2,
    Finished = 3
};

struct Cmp
{
    bool operator()(const std::shared_ptr<Task>& a, const std::shared_ptr<Task>& b) const { return a->execute_after_ > b->execute_after_; }
};

class TimerQueue
{
public:
    TimerQueue() = default;
    void AddTask(std::shared_ptr<Task> task);
    void Shutdown();

private:
    friend void             TimerWorker(TimerQueue* timer);
    std::mutex              mutex_;
    std::condition_variable cv_;

    bool                                                                                working_ = true;
    bool                                                                                update_  = false;
    std::priority_queue<std::shared_ptr<Task>, std::vector<std::shared_ptr<Task>>, Cmp> tasks_;
};

template <class T> class Future : public Task
{
public:
    template <class F> Future(const F& f) : f_(std::move(f)) {}
    virtual ~Future() {}
    virtual void Run() override
    {
        try
        {
            res_ = f_();
        } catch (std::exception& e)
        {
            e_ptr_ = std::current_exception();
        }
    }

    T Get()
    {
        Wait();
        if (e_ptr_)
        {
            std::rethrow_exception(e_ptr_);
        }
        return res_;
    }

private:
    std::exception_ptr e_ptr_ = nullptr;
    T                  res_;
    std::function<T()> f_;
};

class Executor
{
public:
    Executor(size_t num_threads = std::thread::hardware_concurrency());
    ~Executor();

    void Submit(std::shared_ptr<Task> task);

    void StartShutdown();
    void WaitShutdown();

    template <class T> FuturePtr<T> Invoke(std::function<T()> fn)
    {
        FuturePtr<T> task = std::make_shared<Future<T>>(fn);
        Submit(task);
        return task;
    }

    template <class Y, class T> FuturePtr<Y> Then(FuturePtr<T> input, std::function<Y()> fn)
    {
        FuturePtr<Y> task = std::make_shared<Future<Y>>(fn);
        task->AddDependency(input);
        Submit(task);
        return task;
    }

    template <class T> FuturePtr<std::vector<T>> WhenAll(std::vector<FuturePtr<T>> all)
    {
        auto func = [all]()
        {
            std::vector<T> ans;
            for (auto t : all)
            {
                ans.push_back(t->Get());
            }
            return ans;
        };
        FuturePtr<std::vector<T>> task = std::make_shared<Future<std::vector<T>>>(func);
        for (auto t : all)
        {
            task->AddDependency(t);
        }
        Submit(task);
        return task;
    }

    template <class T> FuturePtr<std::vector<T>> WhenAllBeforeDeadline(std::vector<FuturePtr<T>> all, std::chrono::system_clock::time_point deadline)
    {
        auto func = [all]()
        {
            std::vector<T> ans;
            for (auto t : all)
            {
                if (t->IsFinished())
                {
                    ans.push_back(t->Get());
                }
            }
            return ans;
        };
        FuturePtr<std::vector<T>> task = std::make_shared<Future<std::vector<T>>>(func);
        task->SetTimeTrigger(deadline);
        Submit(task);
        return task;
    }

private:
    friend void Worker(size_t worker_id, Executor* executor);
    friend class Task;
    void                  NotifyAvailable();
    std::shared_ptr<Task> WaitForTask();

    TimerQueue                        timer_;
    ExecutorState                     state_;
    std::deque<std::shared_ptr<Task>> tasks_;
    bool                              task_available_ = false;

    std::vector<std::thread> threads_;

    std::mutex              mutex_;
    std::condition_variable wait_for_task_;
};

std::shared_ptr<Executor> MakeThreadPoolExecutor(int num_threads);
