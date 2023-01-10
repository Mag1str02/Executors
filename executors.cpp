#include "executors.h"

#define DEBUG_PRINT 0

#if DEBUG_PRINT == 1
#define PRINT(...) printf(__VA_ARGS__)
#else
#define PRINT(...)
#endif

//*___TASK_____________________________________________________________________________________________________________

void Task::AddDependency(std::shared_ptr<Task> dep) {
    dep->sub_dependency_.push_back(shared_from_this());
    has_dependency_ = true;
    no_condition_ = false;
    ++dependency_count_;
}
void Task::AddTrigger(std::shared_ptr<Task> dep) {
    dep->sub_trigger_.push_back(shared_from_this());
    has_trigger_ = true;
    no_condition_ = false;
}
void Task::SetTimeTrigger(TimePoint at) {
    execute_after_ = at;
    has_timer_ = true;
    no_condition_ = false;
}

bool Task::IsReadyToExecute() {
    bool processing = status_.load() >= TaskStatus::Processing;
    if (processing) {
        return false;
    }
    if (no_condition_) {
        return true;
    }
    if (has_dependency_ && dependency_count_ == finished_dependency_count_.load()) {
        return true;
    }
    if (has_trigger_ && triggered_.load()) {
        return true;
    }
    if (has_timer_ && the_time_has_come_.load()) {
        return true;
    }
    return false;
}
bool Task::IsCompleted() {
    return status_.load() == TaskStatus::Completed;
}
bool Task::IsFailed() {
    return status_.load() == TaskStatus::Failed;
}
bool Task::IsCanceled() {
    return status_.load() == TaskStatus::Canceled;
}
bool Task::IsFinished() {
    return status_.load() >= TaskStatus::Canceled;
}

std::exception_ptr Task::GetError() {
    std::unique_lock lock(mutex_);
    return e_ptr_;
}

void Task::Finish(TaskStatus status, std::exception_ptr exception) {
    status_.store(status);
    for (auto sub_dep : sub_dependency_) {
        sub_dep->finished_dependency_count_.fetch_add(1);
    }
    for (auto sub_trg : sub_trigger_) {
        sub_trg->triggered_.store(1);
    }
    UpdateReadyStatus();
    ResetExecutor();
    {
        std::unique_lock lock(mutex_);
        if (status == TaskStatus::Failed) {
            e_ptr_ = exception;
        }
        finished_ = true;
    }
    for (auto sub_dep : sub_dependency_) {
        sub_dep->UpdateReadyStatus();
    }
    for (auto sub_trg : sub_trigger_) {
        sub_trg->UpdateReadyStatus();
    }
    cv_.notify_all();
}
void Task::Cancel() {
    Finish(TaskStatus::Canceled);
    PRINT("Task cancelled.\n");
}
void Task::SubmitTo(Executor* executor) {
    executor_.store(executor);
}
void Task::ResetExecutor() {
    executor_.store(nullptr);
}
void Task::BeginProcessing() {
    status_.store(TaskStatus::Processing);
}
void Task::UpdateReadyStatus() {
    auto executor = executor_.load();
    if (executor) {
        PRINT("Executor notified.\n");
        executor->NotifyAvailable();
    }
}
void Task::Wait() {
    std::unique_lock<std::mutex> lock(mutex_);
    PRINT("Waiting for task to finish...\n");
    cv_.wait(lock, [this]() { return this->finished_; });
    PRINT("Waiting for task finished.\n");
}

//*___WORKER_______________________________________________________________________________________________________________

void Worker(size_t worker_id, Executor* executor) {
    (void)worker_id;
    while (true) {
        auto task = executor->WaitForTask();
        TaskStatus status = TaskStatus::Completed;
        std::exception_ptr exception = nullptr;
        if (!task) {
            break;
        }
        // printf("Worker %d resieved task.\n", (int)worker_id);
        try {
            task->Run();
            ++executor->works_finished_[worker_id];
            // printf("Worker %d finished task.\n", (int)worker_id);
        } catch (const std::exception& e) {
            PRINT("Task failed.\n");
            status = TaskStatus::Failed;
            exception = std::current_exception();
        }
        task->Finish(status, exception);
    }
    PRINT("%d: Worker finished.\n", (int)worker_id);
    executor->wait_for_task_.notify_one();
}

//*___EXECUTOR_____________________________________________________________________________________________________________

void Executor::Submit(std::shared_ptr<Task> task) {
    std::unique_lock<std::mutex> lock(mutex_);
    if (task->IsCanceled() || state_ >= ExecutorState::ShutDown) {
        return;
    }
    task_available_ = true;
    tasks_.push_back(task);
    task->SubmitTo(this);
    wait_for_task_.notify_one();
    PRINT("Task added to queue.\n");
}

std::shared_ptr<Task> Executor::WaitForTask() {
    std::unique_lock lock(mutex_);
    PRINT("Waiting for task...\n");
    std::shared_ptr<Task> res = nullptr;
    while (true) {
        wait_for_task_.wait(lock, [this]() {
            return this->task_available_ || this->state_ == ExecutorState::Finished;
        });
        bool was_not_finished = false;
        PRINT("Searching task from: %d\n", (int)tasks_.size());
        for (int64_t i = 0; i < static_cast<int64_t>(tasks_.size()); ++i) {
            if (!was_not_finished && tasks_[i]->IsFinished()) {
                tasks_.pop_front();
                PRINT("Dropping empty task.\n");
                --i;
                continue;
            }
            was_not_finished = true;
            if (tasks_[i]->IsReadyToExecute()) {
                res = tasks_[i];
                res->BeginProcessing();
                break;
            }
        }
        if (!res) {
            if (state_ == ExecutorState::Finished) {
                PRINT("Task not found because Executor finished.\n");
                break;
            }
            task_available_ = false;
            PRINT("Task not found.\n");
            continue;
        } else {
            PRINT("Task found.\n");
            break;
        }
    }
    lock.unlock();
    wait_for_task_.notify_one();
    return res;
}

Executor::Executor(size_t num_threads) {
    PRINT("Executor created: %p\n", (void*)this);
    works_finished_.resize(num_threads);
    std::unique_lock lock(mutex_);
    threads_.reserve(num_threads);
    for (size_t i = 0; i < num_threads; ++i) {
        threads_.emplace_back(Worker, i, this);
    }
    state_ = ExecutorState::Working;
}
Executor::~Executor() {
    if (state_ != ExecutorState::Finished) {
        WaitShutdown();
    }
}

void Executor::StartShutdown() {
    PRINT("Starting shutdown.\n");
    std::unique_lock<std::mutex> lock(mutex_);
    state_ = ExecutorState::ShutDown;
}
void Executor::WaitShutdown() {
    PRINT("Waiting for shutdown.\n");
    mutex_.lock();
    state_ = ExecutorState::Finished;
    mutex_.unlock();
    wait_for_task_.notify_all();
    for (size_t i = 0; i < threads_.size(); ++i) {
        threads_[i].join();
    }
    for (size_t i = 0; i < tasks_.size(); ++i) {
        tasks_[i]->ResetExecutor();
    }
    tasks_.clear();
    PRINT("Shutdown complete.\n");
}
void Executor::NotifyAvailable() {
    std::unique_lock<std::mutex> lock(mutex_);
    task_available_ = true;
    wait_for_task_.notify_one();
}

std::shared_ptr<Executor> MakeThreadPoolExecutor(int num_threads) {
    return std::make_shared<Executor>(num_threads);
}
