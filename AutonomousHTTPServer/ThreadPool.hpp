#pragma once 
#include <iostream>
#include <queue>
#include <pthread.h>
#include "Task.hpp"
#include "Log.hpp"

#define NUM 5

class ThreadPool{
  private:
    ThreadPool(int sock, int num = NUM)
      :threadNum(num),
       stop(false),
       sockfd(sock)
    {}
    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;
  private:  
    void InitThreadPool()
    {
      LOG(INFO, "Initialize ThreadPool");
      pthread_mutex_init(&_mtx, nullptr);
      pthread_cond_init(&_cond, nullptr);
      
      for(int i = 0; i < threadNum; ++i)
      {
        pthread_t tid;
        pthread_create(&tid, nullptr, Routine, this);
        if(tid < 0){
          LOG(FATAL, "thread create faild!");
        }
      }
    }

  public:
    static void* Routine(void* arg)
    {
      ThreadPool* tp = reinterpret_cast<ThreadPool*>(arg);
      while(true)
      {
        tp->LockQueue();
        while(tp->IsEmpty())
        {
          tp->ThreadWait();
        }
        Task* t;
        tp->PopTask(&t);
        tp->UnLockQueue();
        
        t->ProcessOn();  //任务处理一定要在锁外面做
        delete t;
      }
    }

    void PushTask(Task*& task) //这里不能加const，const变量无法取地址
    {
      LockQueue();
      taskQueue.push(task);
      UnLockQueue();
      ThreadWakeUp();
    }

    void PopTask(Task** task)
    {
      *task = taskQueue.front();
      taskQueue.pop();
    }

    ~ThreadPool()
    {
      pthread_mutex_destroy(&_mtx);
      pthread_cond_destroy(&_cond);
    }

    //单例模式
    static ThreadPool* GetInstance(int sock)
    {
      pthread_mutex_t tmp_lock = PTHREAD_MUTEX_INITIALIZER;
      if(nullptr == _inst)
      {
        pthread_mutex_lock(&tmp_lock);
        if(nullptr == _inst)
        {
          _inst = new ThreadPool(sock);
          _inst->InitThreadPool();  //直接在创建对象时候初始化线程池
        }
        pthread_mutex_unlock(&tmp_lock);
      }
      return _inst;
    }

  private:
    void LockQueue() { pthread_mutex_lock(&_mtx); }
    void UnLockQueue() { pthread_mutex_unlock(&_mtx); }
    bool IsEmpty() { return taskQueue.empty(); }
    void ThreadWait() { pthread_cond_wait(&_cond, &_mtx); }
    void ThreadWakeUp() { pthread_cond_signal(&_cond); }
  private:
    std::queue<Task*> taskQueue;
    int threadNum;
    pthread_mutex_t _mtx;
    pthread_cond_t _cond;
    bool stop;
    int sockfd;
    static ThreadPool* _inst;
};

ThreadPool* ThreadPool::_inst = nullptr;
