// Copyright (c) Yasser Asmi
// Released under the MIT License (http://opensource.org/licenses/MIT)

#ifndef _WSTPOOL_H
#define _WSTPOOL_H

#include <list>
#include <deque>
#include <future>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>

namespace wstpool
{

class ThreadPool
{
public:
    ThreadPool(size_t count)
    {
        // Setup workers and start their threads
        for (size_t i = 0; i < count; i++)
        {
            mWorkers.emplace_back(this, 1000 + i);
        }
        mStealIter = mWorkers.begin();
        for (auto& wrk : mWorkers)
        {
            wrk.start();
        }
    }
    ~ThreadPool()
    {
        for (auto& wrk : mWorkers)
        {
            wrk.exit();
        }
    }

	template<typename F, typename... ARG>
	auto async(F&& f, ARG&&... args) -> std::future<typename std::result_of<F(ARG...)>::type>
	{
		using rettype = typename std::result_of<F(ARG...)>::type;

        // Create a callwrapper and packaged task, which has a future object and wrap
        // it into a void func with no param using a lambda
        auto task = std::make_shared<std::packaged_task<rettype()>> (
            std::bind(std::forward<F>(f), std::forward<ARG>(args)...));

        std::function<void()> work =
            [task]()
            {
                (*task)();
            };

        // Push work.  Find the queue for this task based on current thread id, to try
        // to keep the new child task on the same worker thread if possible.
        pushWork(work);

        // Return the task future for syncronization
		return task->get_future();
	}

private:
    template <typename T>
    class Queue
    {
    public:
        void push(T& e)
        {
            std::unique_lock<std::mutex> lock(mQMut);
            mQ.push_back(e);
        }
        bool pop(T& e)
        {
            std::unique_lock<std::mutex> lock(mQMut);
            if (mQ.empty())
            {
                return false;
            }
            e = std::move(mQ.front());
            mQ.pop_front();
            return true;
        }
        bool steal(T& e)
        {
            std::unique_lock<std::mutex> lock(mQMut);
            if (mQ.empty())
            {
                return false;
            }
            e = std::move(mQ.back());
            mQ.pop_back();
            return true;
        }
    private:
        std::deque<T> mQ;
        std::mutex mQMut;
    };

    class Worker
    {
    public:
        Worker() = delete;
        Worker(ThreadPool* pool, int id) :
            mPool(pool),
            mId(id)
        {
        }
        void start()
        {
            mThread = std::thread(&Worker::threadFunc, this);
        }
        void push(std::function<void()>& work)
        {
            mQue.push(work);
            wake();
            mPool->requestSteal();
        }
        void wake()
        {
            mCv.notify_one();
        }
        bool steal(std::function<void()>& work)
        {
            return mQue.steal(work);
        }
        int id()
        {
            return mId;
        }
        std::thread::id threadId()
        {
            return mThread.get_id();
        }
        void exit()
        {
            mExit = true;
            wake();
            mThread.join();
        }

    private:
        std::thread mThread;
        std::condition_variable mCv;
        std::mutex mCvMut;
        Queue<std::function<void()>> mQue;
        int mId;
        ThreadPool* mPool;
        bool mExit = false;

        void threadFunc()
        {
            while (!mExit)
            {
                // Try to get work from either this queue or, if none found, steal work from
                // another task queue
                std::function<void()> work;
                bool gotwork = mQue.pop(work);
                if (!gotwork)
                {
                    gotwork = mPool->stealWork(work, mId);
                }

                // If we have work, do the work, else wait.
                if (gotwork)
                {
                    work();
                }
                else
                {
                    std::unique_lock<std::mutex> lock(mCvMut);
                    mCv.wait(lock);
                }
            }
        }
    };

    std::list<Worker> mWorkers;
    std::list<Worker>::iterator mStealIter;

    void pushWork(std::function<void()>& work)
    {
        std::thread::id thisid = std::this_thread::get_id();
        for (auto& wrk : mWorkers)
        {
            if (thisid == wrk.threadId())
            {
                wrk.push(work);
                return;
            }
        }
        mWorkers.front().push(work);
    }
    bool stealWork(std::function<void()>& work, int excludeId)
    {
        for (auto& wrk : mWorkers)
        {
            if (wrk.id() != excludeId)
            {
                if (wrk.steal(work))
                {
                    return true;
                }
            }
        }
        return false;
    }
    bool requestSteal()
    {
        mStealIter++;
        if (mStealIter == mWorkers.end())
        {
            mStealIter = mWorkers.begin();
        }
        mStealIter->wake();
    }
};

} // wstpool namespace

#endif // _WSTPOOL_H