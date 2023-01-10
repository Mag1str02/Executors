#include <thread>
#include <chrono>
#include <atomic>

#include "executors.h"

// typedef std::function<std::shared_ptr<Executor>()> ExecutorMaker;

// struct ExecutorsTest : public testing::TestWithParam<ExecutorMaker>
// {
//     std::shared_ptr<Executor> pool;

//     ExecutorsTest() { pool = GetParam()(); }
// };

// TEST_P(ExecutorsTest, Destructor) {
// }

// TEST_P(ExecutorsTest, Shutdown) {
//     pool->StartShutdown();
//     pool->WaitShutdown();
// }

// class TestTask : public Task {
// public:
//     bool completed = false;

//     void Run() override {
//         EXPECT_TRUE(!completed) << "Seems like task body was run multiple times";
//         completed = true;
//     }
// };

// class SlowTask : public Task {
// public:
//     std::atomic<bool> completed{false};

//     void Run() override {
//         std::this_thread::sleep_for(std::chrono::milliseconds(1));
//         completed = true;
//     }
// };

// class FailingTestTask : public Task {
// public:
//     void Run() override {
//         throw std::logic_error("Failed");
//     }
// };

// TEST_P(ExecutorsTest, RunSingleTask) {
//     auto task = std::make_shared<TestTask>();

//     pool->Submit(task);

//     task->Wait();

//     EXPECT_TRUE(task->completed);
//     EXPECT_TRUE(task->IsFinished());
//     EXPECT_FALSE(task->IsCanceled());
//     EXPECT_FALSE(task->IsFailed());
// }

// TEST_P(ExecutorsTest, RunSingleFailingTask) {
//     auto task = std::make_shared<FailingTestTask>();

//     pool->Submit(task);

//     task->Wait();

//     EXPECT_FALSE(task->IsCompleted());
//     EXPECT_FALSE(task->IsCanceled());
//     EXPECT_TRUE(task->IsFailed());

//     EXPECT_THROW(std::rethrow_exception(task->GetError()), std::logic_error);
// }

// TEST_P(ExecutorsTest, CancelSingleTask) {
//     auto task = std::make_shared<TestTask>();
//     task->Cancel();
//     task->Wait();

//     EXPECT_FALSE(task->IsCompleted());
//     EXPECT_TRUE(task->IsCanceled());
//     EXPECT_FALSE(task->IsFailed());

//     pool->Submit(task);
//     task->Wait();

//     EXPECT_FALSE(task->IsCompleted());
//     EXPECT_TRUE(task->IsCanceled());
//     EXPECT_FALSE(task->IsFailed());
// }

// TEST_P(ExecutorsTest, TaskWithSingleDependency) {
//     auto task = std::make_shared<TestTask>();
//     auto dependency = std::make_shared<TestTask>();

//     task->AddDependency(dependency);

//     pool->Submit(task);

//     std::this_thread::sleep_for(std::chrono::milliseconds(1));
//     EXPECT_FALSE(task->IsFinished());

//     pool->Submit(dependency);

//     task->Wait();
//     EXPECT_TRUE(task->IsFinished());
// }

// TEST_P(ExecutorsTest, TaskWithSingleCompletedDependency) {
//     auto task = std::make_shared<TestTask>();
//     auto dependency = std::make_shared<TestTask>();

//     task->AddDependency(dependency);

//     pool->Submit(dependency);
//     dependency->Wait();

//     std::this_thread::sleep_for(std::chrono::milliseconds(1));
//     ASSERT_FALSE(task->IsFinished());

//     pool->Submit(task);
//     task->Wait();
//     ASSERT_TRUE(task->IsFinished());
// }

// TEST_P(ExecutorsTest, FailedDependencyIsConsideredCompleted) {
//     auto task = std::make_shared<TestTask>();
//     auto dependency = std::make_shared<FailingTestTask>();

//     task->AddDependency(dependency);

//     pool->Submit(task);

//     std::this_thread::sleep_for(std::chrono::milliseconds(1));
//     EXPECT_FALSE(task->IsFinished());

//     pool->Submit(dependency);

//     task->Wait();
//     EXPECT_TRUE(task->IsFinished());
// }

// TEST_P(ExecutorsTest, CanceledDependencyIsConsideredCompleted) {
//     auto task = std::make_shared<TestTask>();
//     auto dependency = std::make_shared<TestTask>();

//     task->AddDependency(dependency);

//     pool->Submit(task);

//     std::this_thread::sleep_for(std::chrono::milliseconds(1));
//     EXPECT_FALSE(task->IsFinished());

//     dependency->Cancel();

//     task->Wait();
//     EXPECT_TRUE(task->IsFinished());
// }

// struct RecursiveTask : public Task {
//     RecursiveTask(int n, std::shared_ptr<Executor> executor) : n_(n), executor_(executor) {
//     }

//     void Run() {
//         if (n_ > 0) {
//             executor_->Submit(std::make_shared<RecursiveTask>(n_ - 1, executor_));
//         }

//         if (n_ == 0) {
//             executor_->StartShutdown();
//         }
//     }

// private:
//     const int n_;
//     const std::shared_ptr<Executor> executor_;
// };

// TEST_P(ExecutorsTest, RunRecursiveTask) {
//     auto task = std::make_shared<RecursiveTask>(100, pool);
//     pool->Submit(task);

//     pool->WaitShutdown();
// }

// TEST_P(ExecutorsTest, TaskWithSingleTrigger) {
//     auto task = std::make_shared<TestTask>();
//     auto trigger = std::make_shared<TestTask>();

//     task->AddTrigger(trigger);
//     pool->Submit(task);

//     std::this_thread::sleep_for(std::chrono::milliseconds(100));
//     EXPECT_FALSE(task->IsFinished());

//     pool->Submit(trigger);

//     task->Wait();
//     EXPECT_TRUE(task->IsFinished());
// }

// TEST_P(ExecutorsTest, TaskWithSingleCompletedTrigger) {
//     auto task = std::make_shared<TestTask>();
//     auto trigger = std::make_shared<TestTask>();

//     task->AddTrigger(trigger);
//     pool->Submit(trigger);
//     trigger->Wait();

//     EXPECT_FALSE(task->IsFinished());

//     pool->Submit(task);

//     task->Wait();
//     EXPECT_TRUE(task->IsFinished());
// }

// TEST_P(ExecutorsTest, TaskWithTwoTrigger) {
//     auto task = std::make_shared<TestTask>();
//     auto trigger_a = std::make_shared<TestTask>();
//     auto trigger_b = std::make_shared<TestTask>();

//     task->AddTrigger(trigger_a);
//     task->AddTrigger(trigger_b);

//     pool->Submit(task);
//     pool->Submit(trigger_b);
//     pool->Submit(trigger_a);

//     task->Wait();
//     EXPECT_TRUE(task->IsFinished());
// }

// TEST_P(ExecutorsTest, MultipleDependencies) {
//     auto task = std::make_shared<TestTask>();
//     auto dep1 = std::make_shared<TestTask>();
//     auto dep2 = std::make_shared<TestTask>();

//     task->AddDependency(dep1);
//     task->AddDependency(dep2);

//     pool->Submit(task);

//     std::this_thread::sleep_for(std::chrono::milliseconds(1));
//     EXPECT_FALSE(task->IsFinished());

//     pool->Submit(dep1);
//     dep1->Wait();

//     std::this_thread::sleep_for(std::chrono::milliseconds(1));
//     EXPECT_FALSE(task->IsFinished());

//     pool->Submit(dep2);
//     task->Wait();

//     EXPECT_TRUE(task->IsFinished());
// }

// TEST_P(ExecutorsTest, TaskWithSingleTimeTrigger) {
//     auto task = std::make_shared<TestTask>();

//     task->SetTimeTrigger(std::chrono::system_clock::now() + std::chrono::milliseconds(200));

//     pool->Submit(task);

//     std::this_thread::sleep_for(std::chrono::milliseconds(100));
//     EXPECT_FALSE(task->IsFinished());

//     task->Wait();
//     EXPECT_TRUE(task->IsFinished());
// }

// TEST_P(ExecutorsTest, DISABLED_TaskTriggeredByTimeAndDep) {
//     auto task = std::make_shared<TestTask>();
//     auto dep = std::make_shared<TestTask>();

//     task->AddDependency(dep);
//     task->SetTimeTrigger(std::chrono::system_clock::now() + std::chrono::milliseconds(2));

//     pool->Submit(task);
//     pool->Submit(dep);

//     std::this_thread::sleep_for(std::chrono::milliseconds(1));
//     EXPECT_TRUE(task->IsFinished());

//     std::this_thread::sleep_for(std::chrono::milliseconds(3));
// }

// TEST_P(ExecutorsTest, MultipleTimerTriggers) {
//     auto task_a = std::make_shared<TestTask>();
//     auto task_b = std::make_shared<TestTask>();

//     task_a->SetTimeTrigger(std::chrono::system_clock::now() + std::chrono::milliseconds(50));
//     task_b->SetTimeTrigger(std::chrono::system_clock::now() + std::chrono::milliseconds(1));

//     pool->Submit(task_a);
//     pool->Submit(task_b);

//     task_a->Wait();
//     EXPECT_TRUE(task_b->IsFinished());
// }

// TEST_P(ExecutorsTest, MultipleTimerTriggersWithReverseOrder) {
//     auto task_a = std::make_shared<TestTask>();
//     auto task_b = std::make_shared<TestTask>();

//     task_a->SetTimeTrigger(std::chrono::system_clock::now() + std::chrono::milliseconds(50));
//     task_b->SetTimeTrigger(std::chrono::system_clock::now() + std::chrono::milliseconds(1));

//     pool->Submit(task_b);
//     pool->Submit(task_a);

//     task_b->Wait();
//     EXPECT_FALSE(task_a->IsFinished());
// }

// TEST_P(ExecutorsTest, PossibleToCancelAfterSubmit) {
//     std::vector<std::shared_ptr<SlowTask>> tasks;
//     for (int i = 0; i < 1000; ++i) {
//         auto task = std::make_shared<SlowTask>();
//         tasks.push_back(task);

//         pool->Submit(task);

//         task->Cancel();
//     }

//     pool.reset();

//     for (auto t : tasks) {
//         if (!t->completed) {
//             return;
//         }
//     }

//     FAIL() << "Seems like Cancel() doesn't affect Submitted tasks";
// }

// TEST_P(ExecutorsTest, CancelAfterDetroyOfExecutor) {
//     auto first = std::make_shared<TestTask>();
//     auto second = std::make_shared<TestTask>();
//     first->AddDependency(second);
//     pool->Submit(first);

//     pool.reset();

//     first->Cancel();
//     second->Cancel();
// }

// struct RecursiveGrowingTask : public Task {
//     RecursiveGrowingTask(int n, int fanout, std::shared_ptr<Executor> executor)
//         : n_(n), fanout_(fanout), executor_(executor) {
//     }

//     void Run() {
//         if (n_ > 0) {
//             for (int i = 0; i < fanout_; ++i) {
//                 executor_->Submit(
//                     std::make_shared<RecursiveGrowingTask>(n_ - 1, fanout_, executor_));
//             }
//         }

//         if (n_ == 0) {
//             executor_->StartShutdown();
//         }
//     }

// private:
//     const int n_, fanout_;
//     const std::shared_ptr<Executor> executor_;
// };

// TEST_P(ExecutorsTest, NoDeadlockWhenSubmittingFromTaskBody) {
//     auto task = std::make_shared<RecursiveGrowingTask>(5, 10, pool);
//     pool->Submit(task);

//     pool->WaitShutdown();
// }

// TEST_P(ExecutorsTest, ManyTasks) {
//     std::vector<std::shared_ptr<Task>> tasks;
//     for (size_t i = 0; i < 1000; ++i) {
//         tasks.push_back(std::make_shared<TestTask>());
//     }
//     for (size_t i = 0; i < 1000; ++i) {
//         pool->Submit(tasks[i]);
//     }

//     pool->WaitShutdown();
// }

// INSTANTIATE_TEST_CASE_P(ThreadPool, ExecutorsTest,
//                         ::testing::Values([] { return MakeThreadPoolExecutor(1); },
//                                           [] { return MakeThreadPoolExecutor(2); },
//                                           [] { return MakeThreadPoolExecutor(10); }));

//*___MY_TESTS______________________________________________________________________________________________________________

// class MyTask : public Task {
// public:
//     virtual void Run() override {
//         for (size_t i = 0; i < 100'000'000; ++i) {
//         }
//     }
// };
// TEST_P(ExecutorsTest, Performance) {
//     auto t = std::chrono::system_clock::now();
//     std::vector<std::shared_ptr<Task>> tasks;
//     for (size_t i = 0; i < 10; ++i) {
//         auto task = std::make_shared<MyTask>();
//         tasks.push_back(task);
//         pool->Submit(task);
//     }
//     auto duration =
//         std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() -
//         t)
//             .count();
//     printf("Submit time elapsed: %dms\n", (int)duration);

//     for (auto task : tasks) {
//         task->Wait();
//     }

// pool->WaitShutdown();

//     duration =
//         std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() -
//         t)
//             .count();
//     printf("Time elapsed: %dms\n", (int)duration);
// }
// TEST_P(ExecutorsTest, PerformanceRaw) {
//     auto t = std::chrono::system_clock::now();
//     std::vector<std::thread> threads;
//     threads.reserve(10);
//     for (size_t i = 0; i < 10; ++i) {
//         threads.emplace_back([]() {
//             for (size_t i = 0; i < 100'000'000; ++i) {
//             }
//         });
//     }
//     auto duration =
//         std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() -
//         t)
//             .count();
//     printf("Submit time elapsed: %dms\n", (int)duration);

//     for (auto& thread : threads) {
//         thread.join();
//     }
//     duration =
//         std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() -
//         t)
//             .count();
//     printf("Time elapsed: %dms\n", (int)duration);
// }
class MyTask : public Task
{
private:
    size_t amount_;
    bool   completed_ = false;

public:
    MyTask(size_t amount) : amount_(amount) {}
    virtual void Run() override
    {
        if (completed_)
        {
            throw std::runtime_error("");
        }
        for (size_t i = 0; i < amount_; ++i)
        {}
        completed_ = true;
    }
};

static auto BenchmarkSimpleCorrectSubmit(size_t num_threads, size_t task_size, size_t task_amount)
{
    auto                               executor = MakeThreadPoolExecutor(num_threads);
    std::vector<std::shared_ptr<Task>> tasks;
    tasks.reserve(task_amount);
    auto t = std::chrono::system_clock::now();
    for (size_t i = 0; i < task_amount; ++i)
    {
        auto task = std::make_shared<MyTask>(task_size);
        tasks.push_back(task);
        executor->Submit(task);
    }
    for (auto task : tasks)
    {
        task->Wait();
    }
    auto res = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - t).count();

    // auto tmp = executor->Statistic();
    // for (size_t i = 0; i < tmp.size(); ++i) {
    //     std::cout << "(" << i << ": " << tmp[i] << ") ";
    // }
    // std::cout << std::endl;
    return res;
}

void TestThreads(size_t num_threads)
{
    size_t ms_count   = 0;
    size_t run_amount = 0;
    while (ms_count < 64'000)
    {
        ms_count += BenchmarkSimpleCorrectSubmit(num_threads, 100'000'000, 64);
        ++run_amount;
    }
    printf("Threads: %d, Runs: %d, Time: %lfms\n", (int)num_threads, (int)run_amount, double(ms_count) / run_amount);
}

int main()
{
    TestThreads(1);
    TestThreads(2);
    TestThreads(4);
    TestThreads(8);
    TestThreads(16);
    TestThreads(24);
    TestThreads(32);
}