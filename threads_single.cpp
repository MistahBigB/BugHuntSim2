// Simple thread demonstration using std::thread to assign tasks directly.
// Still uses a thread vector but assignment has changed.

#include <iostream>
#include <thread>
#include <vector>
#include <mutex>
#include <cstdlib>
#include <ctime>

// shared globak
int globalSum = 0;

std::mutex mtx;
std::mutex printMtx;

// Function to be executed by each thread
void incrementMyVar(int threadID, int iterations) {
    int localSum = 0;   //each thread maintains its own sum
	for (int i = 0; i < iterations; i++) {
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
	std::vector<std::thread> threads;
    const int numThreads = 10;

    srand(time(0));

	//Create threads and assign tasks
	for (int i = 0; i < numThreads; i++) {
        int iterations = rand()  % 10 + 1;
		// Pass the local variable by reference to each thread
		threads.emplace_back(incrementMyVar, i, iterations);
    }

    // Join threads once their work is done
    for (auto& t : threads) {
	    t.join();
    }

    std::cout << "Global sum total is: " << globalSum << std::endl;

    return 0;
}
