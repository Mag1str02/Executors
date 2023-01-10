#include <memory>
#include <chrono>
#include <vector>
#include <functional>
#include <thread>
#include <condition_variable>
#include <deque>
#include <atomic>
#include <mutex>

struct TaskError : public std::runtime_error
{
    using std::runtime_error::runtime_error;
};

#define TASK_ERROR_IF(expr, message) \
    if (expr) throw TaskError(message)

class Executor;
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

class Task : public std::enable_shared_from_this<Task>
{
public:
    using TimePoint = std::chrono::system_clock::time_point;

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
    std::exception_ptr GetError();

    void Cancel();
    void Wait();

private:
    friend void Worker(size_t worker_id, Executor* executor);
    friend class Executor;

    void SubmitTo(Executor* executor);
    void ResetExecutor();
    void BeginProcessing();
    void Finish(TaskStatus status, std::exception_ptr ptr = nullptr);
    void UpdateReadyStatus();

    //*Result
    std::atomic<TaskStatus> status_   = TaskStatus::Unassigned;
    std::atomic<Executor*>  executor_ = nullptr;
    std::exception_ptr      e_ptr_    = nullptr;
    bool                    finished_ = false;

    //*Wait
    std::mutex              mutex_;
    std::condition_variable cv_;

    //*BeforeSubmit
    std::vector<std::shared_ptr<Task>> sub_dependency_;
    std::vector<std::shared_ptr<Task>> sub_trigger_;
    TimePoint                          execute_after_;
    size_t                             dependency_count_ = 0;
    bool                               has_dependency_   = false;
    bool                               has_trigger_      = false;
    bool                               has_timer_        = false;
    bool                               no_condition_     = true;

    //*AfterSubmit
    std::atomic<size_t> finished_dependency_count_ = 0;
    std::atomic<bool>   triggered_                 = false;
    std::atomic<bool>   the_time_has_come_         = false;
};

// template <class T>
// class Future;

// template <class T>
// using FuturePtr = std::shared_ptr<Future<T>>;

// // Used instead of void in generic code
// struct Unit {};

enum class ExecutorState
{
    None     = 0,
    Working  = 1,
    ShutDown = 2,
    Finished = 3
};

class Executor
{
public:
    Executor(size_t num_threads);
    ~Executor();

    void Submit(std::shared_ptr<Task> task);

    void StartShutdown();
    void WaitShutdown();

    // template <class T>
    // FuturePtr<T> Invoke(std::function<T()> fn) {
    // }

    // template <class Y, class T>
    // FuturePtr<Y> Then(FuturePtr<T> input, std::function<Y()> fn);

    // template <class T>
    // FuturePtr<std::vector<T>> WhenAll(std::vector<FuturePtr<T>> all);

    // template <class T>
    // FuturePtr<T> WhenFirst(std::vector<FuturePtr<T>> all);

    // template <class T>
    // FuturePtr<std::vector<T>> WhenAllBeforeDeadline(std::vector<FuturePtr<T>> all,
    //                                                 std::chrono::system_clock::time_point
    //                                                 deadline);
    auto Statistic() { return works_finished_; }

private:
    friend void Worker(size_t worker_id, Executor* executor);
    friend class Task;
    void                  NotifyAvailable();
    std::shared_ptr<Task> WaitForTask();

    ExecutorState                     state_;
    std::deque<std::shared_ptr<Task>> tasks_;
    bool                              task_available_ = false;

    std::vector<std::thread> threads_;
    std::mutex               mutex_;
    std::condition_variable  wait_for_task_;
    std::vector<size_t>      works_finished_;
};

std::shared_ptr<Executor> MakeThreadPoolExecutor(int num_threads);

// template <class T>
// class Future : public Task {
// public:
//     T Get() {
//     }

// private:
// };
