#include "executors.h"

//*___TASK_____________________________________________________________________________________________________________

void Task::AddDependency(std::shared_ptr<Task> dep)
{
    std::unique_lock<std::mutex> lock(dep->mutex_);
    if (dep->IsFinished())
    {
        return;
    }
    dep->sub_dependency_.push_back(weak_from_this());
    has_dependency_ = true;
    no_condition_   = false;
    ++dependency_count_;
}
void Task::AddTrigger(std::shared_ptr<Task> dep)
{
    dep->sub_trigger_.push_back(shared_from_this());
    has_trigger_  = true;
    no_condition_ = false;
}
void Task::SetTimeTrigger(TimePoint at)
{
    execute_after_ = at;
    has_timer_     = true;
    no_condition_  = false;
}

bool Task::IsReadyToExecute()
{
    std::unique_lock<std::mutex> lock(mutex_);
    bool                         processing = status_.load() >= TaskStatus::Processing;
    if (processing)
    {
        return false;
    }
    if (no_condition_)
    {
        return true;
    }
    if (has_dependency_ && dependency_count_ == finished_dependency_count_.load())
    {
        return true;
    }
    if (has_trigger_ && triggered_.load())
    {
        return true;
    }
    if (has_timer_ && the_time_has_come_.load())
    {
        return true;
    }
    return false;
}
bool Task::IsCompleted() { return status_.load() == TaskStatus::Completed; }
bool Task::IsFailed() { return status_.load() == TaskStatus::Failed; }
bool Task::IsCanceled() { return status_.load() == TaskStatus::Canceled; }
bool Task::IsFinished() { return status_.load() >= TaskStatus::Canceled; }
bool Task::HasTimeTrigger() { return has_timer_; }

std::exception_ptr Task::GetError()
{
    std::unique_lock<std::mutex> lock(mutex_);
    return e_ptr_;
}

void Task::Finish(TaskStatus status, std::exception_ptr exception)
{
    std::unique_lock<std::mutex> lock(mutex_);
    status_.store(status);
    lock.unlock();
    for (auto sub_dep : sub_dependency_)
    {
        auto t = sub_dep.lock();
        if (t)
        {
            t->finished_dependency_count_.fetch_add(1);
        }
    }
    for (auto sub_trg : sub_trigger_)
    {
        sub_trg->triggered_.store(1);
    }
    UpdateReadyStatus();
    ResetExecutor();
    {
        std::unique_lock<std::mutex> lock(mutex_);
        if (status == TaskStatus::Failed)
        {
            e_ptr_ = exception;
        }
        finished_ = true;
    }
    for (auto sub_dep : sub_dependency_)
    {
        auto t = sub_dep.lock();
        if (t)
        {
            t->UpdateReadyStatus();
        }
    }
    for (auto sub_trg : sub_trigger_)
    {
        sub_trg->UpdateReadyStatus();
    }
    cv_.notify_all();
}
void Task::Cancel() { Finish(TaskStatus::Canceled); }
void Task::SubmitTo(Executor* executor) { executor_.store(executor); }
void Task::ResetExecutor() { executor_.store(nullptr); }
void Task::BeginProcessing() { status_.store(TaskStatus::Processing); }
void Task::UpdateReadyStatus()
{
    auto executor = executor_.load();
    if (executor)
    {
        executor->NotifyAvailable();
    }
}
void Task::Wait()
{
    std::unique_lock<std::mutex> lock(mutex_);
    cv_.wait(lock, [this]() { return this->finished_; });
}

//*___TIMER________________________________________________________________________________________________________________

void TimerQueue::AddTask(std::shared_ptr<Task> task)
{
    std::unique_lock<std::mutex> lock(mutex_);
    tasks_.push(task);
    update_ = true;
    cv_.notify_one();
}
void TimerQueue::Shutdown()
{
    std::unique_lock<std::mutex> lock(mutex_);
    working_ = false;
    lock.unlock();
    cv_.notify_one();
}

//*___WORKER_______________________________________________________________________________________________________________

void TimerWorker(TimerQueue* timer)
{
    std::unique_lock<std::mutex> lock(timer->mutex_);
    while (true)
    {
        if (!timer->working_)
        {
            return;
        }
        TimePoint t = std::chrono::system_clock::now() + std::chrono::seconds(1000000);
        if (!timer->tasks_.empty())
        {
            t = timer->tasks_.top()->execute_after_;
        }
        else
        {
        }
        auto status = timer->cv_.wait_until(lock, t);
        if (!timer->working_)
        {
            return;
        }
        if (status == std::cv_status::timeout)
        {
            timer->tasks_.top()->the_time_has_come_.store(true);
            timer->tasks_.top()->UpdateReadyStatus();
            timer->tasks_.pop();
            continue;
        }
        if (timer->update_)
        {
            timer->update_ = false;
            continue;
        }
    }
}
void Worker(size_t worker_id, Executor* executor)
{
    (void)worker_id;
    while (true)
    {
        auto               task      = executor->WaitForTask();
        TaskStatus         status    = TaskStatus::Completed;
        std::exception_ptr exception = nullptr;
        if (!task)
        {
            break;
        }
        try
        {
            task->Run();
        } catch (const std::exception& e)
        {
            status    = TaskStatus::Failed;
            exception = std::current_exception();
        }
        task->Finish(status, exception);
    }
    executor->wait_for_task_.notify_one();
}

//*___EXECUTOR_____________________________________________________________________________________________________________

void Executor::Submit(std::shared_ptr<Task> task)
{
    std::unique_lock<std::mutex> lock(mutex_);
    if (task->IsCanceled() || state_ >= ExecutorState::ShutDown)
    {
        return;
    }
    if (task->HasTimeTrigger())
    {
        lock.unlock();
        timer_.AddTask(task);
        lock.lock();
    }
    task_available_ = true;
    tasks_.push_back(task);
    task->SubmitTo(this);
    wait_for_task_.notify_one();
}

std::shared_ptr<Task> Executor::WaitForTask()
{
    std::unique_lock<std::mutex> lock(mutex_);
    std::shared_ptr<Task>        res = nullptr;
    while (true)
    {
        wait_for_task_.wait(lock, [this]() { return this->task_available_ || this->state_ == ExecutorState::Finished; });
        bool was_not_finished = false;
        for (int64_t i = 0; i < static_cast<int64_t>(tasks_.size()); ++i)
        {
            if (!was_not_finished && tasks_[i]->IsFinished())
            {
                tasks_.pop_front();
                --i;
                continue;
            }
            was_not_finished = true;
            if (tasks_[i]->IsReadyToExecute())
            {
                res = tasks_[i];
                res->BeginProcessing();
                break;
            }
        }
        if (!res)
        {
            if (state_ == ExecutorState::Finished)
            {
                break;
            }
            task_available_ = false;
            continue;
        }
        else
        {
            break;
        }
    }
    lock.unlock();
    wait_for_task_.notify_one();
    return res;
}

Executor::Executor(size_t num_threads)
{
    std::unique_lock<std::mutex> lock(mutex_);
    threads_.reserve(num_threads + 1);
    for (size_t i = 0; i < num_threads; ++i)
    {
        threads_.emplace_back(Worker, i, this);
    }
    threads_.emplace_back(TimerWorker, &timer_);
    state_ = ExecutorState::Working;
}
Executor::~Executor()
{
    if (state_ != ExecutorState::Finished)
    {
        WaitShutdown();
    }
}

void Executor::StartShutdown()
{
    std::unique_lock<std::mutex> lock(mutex_);
    state_ = ExecutorState::ShutDown;
}
void Executor::WaitShutdown()
{
    mutex_.lock();
    state_ = ExecutorState::Finished;
    mutex_.unlock();
    wait_for_task_.notify_all();
    for (size_t i = 0; i < threads_.size() - 1; ++i)
    {
        threads_[i].join();
    }
    for (size_t i = 0; i < tasks_.size(); ++i)
    {
        tasks_[i]->ResetExecutor();
    }
    timer_.Shutdown();
    threads_.back().join();
    tasks_.clear();
}
void Executor::NotifyAvailable()
{
    std::unique_lock<std::mutex> lock(mutex_);
    task_available_ = true;
    wait_for_task_.notify_one();
}

std::shared_ptr<Executor> MakeThreadPoolExecutor(int num_threads) { return std::make_shared<Executor>(num_threads); }
