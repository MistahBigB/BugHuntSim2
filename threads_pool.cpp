// Simple thread demonstration using a thread pool and queue
// There are more elaborate solutions to make the global print
// update on time without the wait. Additionally, it would be interesting
// to see how the sum could be updated as the threads run, but locking
// after every update defeats the purpose of multithreading.

#include <iostream>
#include <thread>
#include <vector>
#include <mutex>
#include <cstdlib>
#include <ctime>
#include <queue>
#include <condition_variable>
#include <functional>
#include <atomic>

class ThreadPool {
private:
    std::vector<std::thread> workers;           // Threads in the pool
    std::queue<std::function<void()>> tasks;    // Task queue
    std::mutex queueMutex;                      // Mutex to protect the task queue
    std::condition_variable condition;          // Signals tasks are available

public:
    std::atomic<bool> stop {false};             // Flag to stop the pool
    
    ThreadPool(size_t numThreads){
        for (size_t i = 0; i < numThreads; i++) {
            workers.emplace_back([this]() {
                while(true) {
                    std::function<void()> task;

                    {
                        std::unique_lock<std::mutex> lock(this->queueMutex);

                        // Wait until the queue has tasks or the stop is true
                        this->condition.wait(lock, [this]() {
                            return !this->tasks.empty() || this->stop;
                        });

                        if (this->stop && this->tasks.empty()) return;

                        //Get the next task from the queue
                        task = std::move(this->tasks.front());
                        this->tasks.pop();
                    }

                    task();
                }
            });
        }
    }

    ~ThreadPool(){
        {
            std::unique_lock<std::mutex> lock(queueMutex);
            stop = true;
        }

        condition.notify_all();

        for (std::thread& worker : workers) {
            worker.join();
        }
    }

    void enqueueTask(std::function<void()> task) {
        {
            std::unique_lock<std::mutex> lock(queueMutex);
            tasks.push(std::move(task));
        }
        condition.notify_one();
    }


};


// shared globak
int globalSum = 0;

std::mutex mtx;
std::mutex printMtx;

// Function to be executed by each thread
void incrementMyVar(int threadID, int iterations) {
    int localSum = 0;   //each thread maintains its own sum
	for (int i = 1; i < iterations; i++) {
		localSum += threadID;		//each thread increments its local variable
    }

    // Lock and update the global sum 
    // CAN be done this way to reduce complexity, but now threads cannot sum in parallel, reducing performance
    {
        std::lock_guard<std::mutex> lock(mtx);
        globalSum += localSum;
        //std::cout << "Thread " << threadID << " completed with local sum: " << localSum << std::endl;
    }

    // Lock and print the global sum
    // CAN be done this way to thread sum in parallel, but increases complexity and contention for the lock
    {
        std::lock_guard<std::mutex> lock(printMtx);
        std::cout << "Thread " << threadID << " completed with local sum: " << localSum << std::endl;
    }

}

int main() {
    const int numThreads = 10;
    const int numTasks = 10;
    ThreadPool pool(numThreads);

    srand(time(0));

	// Submit tasks to the thread pool
	for (int i = 0; i < numTasks; i++) {
        int iterations = rand()  % 10 + 1;
		// Pass the local variable by reference to each thread
		pool.enqueueTask([i, iterations]() {
            incrementMyVar(i, iterations);
        });
    }


    // Allow some time for all tasks to complete
    // By far the easiest way to get this to print the right way
    std::this_thread::sleep_for(std::chrono::seconds(1));
    std::cout << "Global sum total is: " << globalSum << std::endl;

    return 0;
}
