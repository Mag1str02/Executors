# Executors

Executors is a small C++ library for multi-threaded execution of tasks. It provides several basic objects for this and also allows you to build various dependencies between tasks


## About

There are 2 main objects in the library:
* `Executor` - an object responsible for managing worker threads and distributing tasks between them
* `Task` - a unit of execution. Represents user-defined payload for submitting to executor

Usually the user creates `Executor` with specified number of worker threads and then creates tasks by inheriting `Task` class. After that several dependency types can be added:

* `Dependency` - the task should start executing after all its dependencies have completed. Represented by `AddDependency` method of `Task`
* `Trigger` - the task should start executing aftel at least one of its triggers have completed. Represented by `AddTrigger` method of `Task`
* `TimeTrigger` - the task should start executing after the time has reached the specified time point. Represented by `SetTimeTrigger` method of `Task`. Note that a task can only have one time trigger

Then task can be added to execution queue of `Executor` via `Submit` method.
The execution of the task will be started after satisfying any of the dependency types or will start immediately if there are no dependencies at all. Finally, the completion of the task execution can be awaited via `Wait` method or task can be canceled via `Cancel` method. Note that if the task execution has already started, it will not be canceled 

Task also has status related methods:
* `IsReadyToExecute` - returns true if the task is ready to be executed(if the above condition is met)
* `IsCompleted` - returns true if the task was successfully executed
* `IsCanceled` - returns true if the task was submitted to an executor and then canceled via `Cancel` method
* `IsFailed` - returns true if an exception was thrown during task execution. The caught exception can be retrieved via `GetError` method
* `IsFinished` - returns true if the task either completed, failed or canceled
* `HasTimeTrigger` - returns true if the task already has a time trigger

In the end executor can stop accepting new task via `StartShutdown` method (all newly submitted tasks will be canceled immediately) and/or wait for all submitted tasks to finish via `WaitShutdown`. Note that, if `WaitShutdown` was not called before executor destruction it will be implicitly called in the destructor

Here is an example of task usage:

```c++
class PrimeTestTask : public Task
{
    int n_;

public:
    PrimeTestTask(int n) : n_(n) {}
    bool         is_prime = false;
    virtual void Run()
    {
        is_prime = true;
        for (size_t i = 2; i * i < n_; ++i)
        {
            if (n_ % i == 0)
            {
                is_prime = false;
                break;
            }
        }
    }
};

class AggregateTask : public Task
{
private:
    std::vector<std::shared_ptr<PrimeTestTask>> tasks_;

public:
    int cnt = 0;
    AggregateTask() {}
    void AddPrimeTask(std::shared_ptr<PrimeTestTask> task)
    {
        tasks_.push_back(task);
        AddDependency(task);
    }
    virtual void Run()
    {
        for (const auto& task : tasks_)
        {
            if (task->is_prime)
            {
                ++cnt;
            }
        }
    }
};

void CountPrimes(int from, int to)
{
    std::shared_ptr<Executor> pool          = std::make_shared<Executor>();
    auto                      agregate_task = std::make_shared<AggregateTask>();
    for (int i = from; i < to; ++i)
    {
        auto task = std::make_shared<PrimeTestTask>(i);
        agregate_task->AddPrimeTask(task);
        pool->Submit(task);
    }
    pool->Submit(agregate_task);
    agregate_task->Wait();
    std::cout << agregate_task->cnt << std::endl;
}
```

There is also `Future` class which inherits `Task` class. It allows to create tasks from lambda functions and stores possible return values. Usually `Future` is used indirectly via executor methods:

* `Invoke` - simply creates task from lambda function, submits it and returns `FuturePtr`
* `Then` - same as `Invoke`, but adds specified dependency before submitting task
* `WhenAll` -  allows to gather results of multiple tasks into single vector and await for all tasks to finish
* `WhenAllBeforeDeadline` - same as `WhenAll`, but after the deadline, all incomplete task results are discarded

Here is the example described above rewritten using `Future` and `Executor` methods:

```c++
void CountPrimes(int from, int to)
{
    std::vector<FuturePtr<bool>> tasks;
    std::shared_ptr<Executor>    pool = std::make_shared<Executor>();
    for (int n = from; n < to; ++n)
    {
        tasks.push_back(pool->Invoke<bool>(
            [n]() -> bool
            {
                for (size_t i = 2; i * i < n; ++i)
                {
                    if (n % i == 0)
                    {
                        return false;
                    }
                }
                return true;
            }));
    }
    auto all_completed = pool->WhenAll(tasks);
    auto res           = pool->Then<int, std::vector<bool>>(all_completed,
        [all_completed]() -> int
        {
            auto primes = all_completed->Get();
            int  cnt    = 0;
            for (const auto& b : primes)
            {
                cnt += b;
            }
            return cnt;
        });
    std::cout << res->Get() << std::endl;
}
```

## Build

Build process is very simple and will add 5 targets to your cmake project: 
* `executors` - library, to use in your project
* `test_executors` - binary for testing the correctness of executors library
* `bench_executors` - binary for testing performance of executors library
* `gmock` - test_executors dependency
* `benchmark` - bench_executors dependency

To build library:

1. Start by cloning the repository with `git clone https://github.com/Mag1str02/Executors` or adding submodule by `git submodule add https://github.com/Mag1str02/Executors`
2. Create build directory by `mkdir build`
3. Move to build dir by `cd build`
4. Configure by `cmake ..`
5. Build by `cmake --build .` 