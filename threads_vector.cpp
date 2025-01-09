// Simple thread demonstration creating a vector of threads to perform
// individual incrementation and then updating a global.

#include <iostream>
#include <thread>
#include <vector>
#include <mutex>
#include <ctime>

int myVar = 0;

std::mutex mtx;

// Function to be executed by each thread
void incrementMyVar(int threadNum, int& threadVar) {
	int iterations = rand() + threadNum;
	for (int i = 0; i < iterations; i++) {
		threadVar++;		//each thread increments its local variable
    }	
}

int main() {
	std::vector<std::thread> threads;
	std::vector<int> threadVars(10,0);	//create a vector to store 10 thread’s local var, init at 0
    srand(time(0));

	//Create threads
	for (int i = 0; i < threadVars.size(); i++) {
		// Pass the local variable by reference to each thread
		threads.emplace_back(incrementMyVar, i, std::ref(threadVars[i]));
    }

    // Join threads once their work is done
    for (auto& t : threads) {
	    t.join();
    }

    for (int i = 0; i < threadVars.size(); i++){
        std::cout << threadVars[i] << " for thread " << i << ".\n";
    }

    // Lock and add all the local vars together
    int total = 0;
    for (const auto& threadVar : threadVars) {
	{
	    std:: lock_guard<std::mutex> lock(mtx);
        myVar+= threadVar;
    }
    }

    std::cout << "Global myVar total is: " << myVar << std::endl;

    return 0;
}
