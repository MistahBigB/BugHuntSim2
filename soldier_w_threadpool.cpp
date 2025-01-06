// This example demonstrates multithreading for combat.
// This version uses a limited number of threads for the threadpool, determined at runtime
// by the number of cores on the machine or the number of combtatants. 
// Added a naming convention, so we can see who's shooting whom, total hits, total kills.
// Race conditions still exist for this scoring, despite the atomic set of dead. 
// All data processing for stats is done in postProcessing().

// TODO: lots of collisions still happening, eg marines are killing one bug and it
// translates to the kiling of the entire swarm.
// Need a separate thread for logging.
// v0.05

#include <iostream>
#include <vector>
#include <thread>
#include <atomic>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <future>
#include <string>

template<typename T>
class ThreadSafeQueue {
public:
    //Push a task to the queue
    void push(const T& item) {
        std::lock_guard<std::mutex> loc(mtx);
        queue.push(item);
        cv.notify_one();    //notifiy one waiting thread
    }

    // Threads pop a task off the queue
    T pop() {
        std::unique_lock<std::mutex> lock(mtx);
        // capture ThreadSafeQueue member variables, no params
        cv.wait(lock, [this]() {return !queue.empty(); });  // lock if the queue is not empty
        T item = queue.front();
        queue.pop();
        return item;
    }

    bool empty() {
        std::lock_guard<std::mutex> lock(mtx);
        return queue.empty();
    }

private:
    std::queue<T> queue;
    std::mutex mtx;
    std::condition_variable cv;
};

using Task =  std::function<void()>;

// This class manages a pool of threads, each of which continuously pulls tasks
// from the queue and executes them. Tasks are submitted using submit().
class ThreadPool {
public:
    ThreadPool(size_t numThreads) : stopFlag(false) {
        //start worker threads
        for (size_t i = 0; i < numThreads; ++i) {
            workers.push_back(std::thread(&ThreadPool::worker, this));
        }
    }

    ~ThreadPool(){
        {
            std::lock_guard<std::mutex> lock(mtx);
            stopFlag = true;
        }
        cv.notify_all();
        for (std::thread& worker : workers) {
            if (worker.joinable()) {
                worker.join();
            }
        }
    }

    void submit (Task task) {
        taskQueue.push(task);
        cv.notify_one();
    }

private:
    void worker() {
        while (true) {
            Task task;
            {
                std::unique_lock<std::mutex> lock(mtx);
                cv.wait(lock, [this]() {return stopFlag || !taskQueue.empty(); });
                if (stopFlag && taskQueue.empty()) return;
                task = taskQueue.pop();
            }
            task(); //execute the task
        }
    }

    std::vector<std::thread> workers;
    ThreadSafeQueue<Task> taskQueue;
    std::mutex mtx;
    std::condition_variable cv;
    bool stopFlag;
};

class Soldier{
public:
    std::string name;
    int health;
    int base_to_hit = 10;
    int hits = 0;
    int kill_count = 0;
    std::vector<std::string> enemies_killed;

    // With atomic r/w, a thread always sees a consisent state of the var. Does not require locks.
    std::atomic<bool> alive;

    // Constructor to initialize attributes
    // Needs to be initialized BEFORE functions pass it as an arg
    Soldier(const std::string& name) : name(name), health(100), alive(true) {}
    
    // Pure virtual method as each soldier will have a different attack
    virtual void attack(Soldier* target) = 0;

    // Default one-shot
    virtual void slay(Soldier* target) {
        int damage = 100;  // Default damage
        target->takeDamage(damage);
        std::cout << "Critical hit! " << name << " takes " << damage << " damage.\n";
    }

    virtual void takeDamage(int damage) {
        health -= damage;
        if (health <= 0) {
            alive = false;
            std::cout << name << " has been killed.\n";
        }
    }

    // Virtual destructor
    virtual ~Soldier() {} 
};

class Marine : public Soldier {
public:
    int accuracy = 7;
    int damage = 50;
    //int marine_hit = 0;

    Marine(const std::string& name) : Soldier(name) {}  // Pass name to Soldier constructor

    // Modifying the standard attack
    void attack(Soldier* target) override {
        std::cout << name << " is shooting...\n";
        int to_hit = rand() % 10 + 1;

        if (to_hit > accuracy){
            std::cout << name << " hits!\n";
            hits++;
            if (to_hit == base_to_hit) {
                slay(target);
            } else {
                target->takeDamage(damage);     // Standard hit
            }
        } else {
            std::cout << name << " misses...\n";
        }
    }

    void slay(Soldier* target) override {
        std::cout << name << " scores a headshot!\n";
        target->takeDamage(damage*2);    // Critical hit
    }
};

class Bug : public Soldier {
public:
    int accuracy = 8;
    int damage = 50;
    bool carapace = true;
    //int bug_hit = 0;
    
    Bug(const std::string& name) : Soldier(name) {}  // Pass name to Soldier constructor

    void attack(Soldier* target) override {
        std::cout << name << " attacks with its claws...\n";
        int to_hit = rand() % 10 + 1;

        if (to_hit > accuracy) {
            std::cout << name << " hits!\n";
            hits++;
            if (to_hit == base_to_hit) {
                slay(target);
            } else {
                target->takeDamage(damage);
            }
        } else {
        std::cout << name << " misses...\n";
        }
    }

    void slay(Soldier* target) override {
        std::cout << name << " finds a gap in the Marine's armor!\n";
        target->health -= (damage*2);
    }
    void takeDamage(int damage) override {
        health -= damage;
        if (health <= 0 && carapace) {
            std::cout << name << "'s carapace protected it from a killing blow!\n";
            health += 50; // Restore some health
            carapace = false;
        } else if (health <= 0) {
            alive = false;
            std::cout << name << " has fallen!\n";
        }
    }
};

void gameLoop(std::vector<std::unique_ptr<Marine>>& marineCorps, std::vector<std::unique_ptr<Bug>>& bugSwarm, ThreadPool& pool) {
    std::atomic<bool> gameOver = false;
    size_t marineCount = marineCorps.size();
    size_t bugCount = bugSwarm.size();
    int sleep_time = 100;
    
    std::cout << "This fight is between " << marineCount << " Marines and " << bugCount << " Bugs!\n"; 

    // Combat lambda for turn based combat 
    auto battle = [&gameOver, &marineCount, &bugCount](Soldier* attacker, Soldier* defender, bool isMarineAttacking) {
        if(!attacker->alive || !defender->alive) return;
        
        attacker->attack(defender);

        // Tried to simply check if the defender was alive, but that led to race conditions
        //if (!defender->alive) {

        // Check if the defender survived the attack
        // This solution isn't better for 
        if (defender->health <= 0) {
            // Attempt to atomically mark the defender as dead
            // If this thread successfully marks the defender as dead, it gets the kill
            if(!defender->alive.exchange(false)) {
                attacker->enemies_killed.push_back(defender->name);
                std::cout << attacker->name << " scores a kill on " << defender->name << "!\n";

                // Update the counters and check for gameOver
                if (isMarineAttacking) {
                    --bugCount;
                    std::cout << bugCount << " bugs remain!\n";
                    if (bugCount == 0) gameOver = true;
                } else {
                    --marineCount;
                    std::cout << marineCount << " Marines remain!\n";
                    if (marineCount == 0) gameOver = true;
                }
            }
        }
    };
    
    // Instead of dynamically creating a vector of threads, this version submits tasks to a pool of threads
    while (!gameOver) {
        // Marines attack Bugs
        for (auto& marine : marineCorps) {
            // Create a new thread and add it to the threads vector
            // emplace_back() directly constructs the thread obj in the vector without creating and copying a temp thread obj
            // [&] captures variables from the surrounding sopr by reference (can access gameOver, bugSwarm, and battle())
            if (marine->alive) {
                pool.submit([&marine, &bugSwarm, &battle, &gameOver, sleep_time]() {
                    while (!gameOver) {
                        // Choose a random target from the opposing team
                        Soldier* target = bugSwarm[rand() % bugSwarm.size()].get();

                        // Calls battle() with the pointer to the Marine, the target, and true for isMarineAttacking
                        battle(marine.get(), target, true);
                        std::this_thread::sleep_for(std::chrono::milliseconds(sleep_time));
                    }
                });
            }
        }

        // Bugs attack Marines
        for (auto& bug : bugSwarm) {
            if (bug->alive) {
                pool.submit([&bug, &marineCorps, &battle, &gameOver, sleep_time]() {
                    while (!gameOver) {
                        Soldier* target = marineCorps[rand() % marineCorps.size()].get();

                        battle(bug.get(), target, false);
                        std::this_thread::sleep_for(std::chrono::milliseconds(sleep_time));
                    }
                });
            } 
        }

        // Allow some time for tasks to process
        std::this_thread::sleep_for(std::chrono::milliseconds(sleep_time*5));
    
        if (gameOver) break;
    }

    // Only in death does duty end.
    if (marineCount > 0) {
        std::cout << "Marine victory!\n";
    } else {
        std::cout << "Bugs triumphant!\n";
    }
}

void postProcessing(const std::vector<std::unique_ptr<Marine>>& marineCorps, 
                    const std::vector<std::unique_ptr<Bug>>& bugSwarm) {

    int total_marine_hits = 0, total_bug_hits = 0;
    int total_marine_kills = 0, total_bug_kills = 0;
    int highest_kill_count = 0;
    std::vector<Soldier*> top_killers;

    auto tallyKills = [](const auto& force, int& total_hits, int& total_kills, int& highest_kill_count, std::vector<Soldier*>& top_killers) {
        int kill_count = 0;
        for (const auto& soldier : force) {
            total_hits += soldier->hits;
            kill_count = soldier->enemies_killed.size();    //reset kill count every soldier
            total_kills += kill_count;
            if (kill_count > highest_kill_count) {
                highest_kill_count = kill_count;
                top_killers.clear();
                top_killers.push_back(soldier.get());
            } else if (kill_count == highest_kill_count) {
                top_killers.push_back(soldier.get());
            }
            }
        };

    tallyKills(marineCorps, total_marine_hits, total_marine_kills, highest_kill_count, top_killers);
    tallyKills(bugSwarm, total_bug_hits, total_bug_kills, highest_kill_count, top_killers);
    

    std::cout << "\nPost Fight Stats!\n";    
    std::cout << "Total Marine hits this fight: " << total_marine_hits << ".\n";
    std::cout << "Total Bug hits this fight: " << total_bug_hits << ".\n";

    if (total_marine_hits > total_bug_hits) {
        std::cout << "The Marines had superior accuracy this battle!\n";
    } else if (total_bug_hits > total_marine_hits) {
        std::cout << "The Bugs were more effective in landing hits!\n";
    } else {
        std::cout << "It's a draw in terms of hit counts!\n";
    }

    auto processForce = [](const auto& force) {
        for (const auto& soldier : force) {
            if (soldier->enemies_killed.empty()) {
            std::cout << soldier->name << " killed: None\n";
            } else {
                std::cout << soldier->name << " killed: ";
                for (const auto& enemy : soldier->enemies_killed) {
                    std::cout << enemy << " ";
                }
                std::cout << "\n";
            }
        }
    };

    std::cout << "\nMarine performance:\n";
    processForce(marineCorps);

    std::cout << "\nBug Performance:\n";
    processForce(bugSwarm);
}

int main() {
    int marine_num = 0;
    int bug_num = 0;

    // Get the number of cores on this system. Thread count should max at double this number, -1 for the OS.
    int numCores = std::thread::hardware_concurrency();
    
    //seed the random number generator with the current time
    srand(time(0));

    // Create vectors to hold different soldier types
    std::vector<std::unique_ptr<Marine>> marineCorps;
    std::vector<std::unique_ptr<Bug>> bugSwarm;

    std::cout << "In the grimdark winter of New England, man dreams of endless war with non-man...\n";
    std::cout << "This is a battle simulation of Marines vs Bugs, oorah!\n";
    
    while (marine_num == 0) {
        std::cout << "How many Marines should fight today?\n";
        std::cin >> marine_num;
        if (marine_num < 1) {
            std::cout << "Troop count must be greater than 0.\n";
        }
    }    

    while (bug_num == 0) {
        std::cout << "How many Bugs should fight today?\n";
        std::cin >> bug_num;
        if (bug_num < 1) {
            std::cout << "Troop count must be greater than 0.\n";
        }
    }

    // Construct a ThreadPool pool such that if marine + bug num > numCores-1, default to number of cores -1
    // Else, the pool will be the sume of the combatants
    ThreadPool pool(
        (marine_num + bug_num) > (numCores - 1) ? (numCores-1) : marine_num + bug_num 
    );

    for (int i = 1; i <= marine_num; i++) {
            //std::string marine_name = "Marine" + std::to_string(i);
            //marineCorps.emplace_back(std::make_unique<Marine>(marine_name));
            marineCorps.emplace_back(std::make_unique<Marine>("Marine" + std::to_string(i)));
    }

    for (int i = 1; i <= bug_num; i++) {
            //std::string bug_name = "Bug" + std::to_string(i);
            //bugSwarm.emplace_back(std::make_unique<Bug>(bug_name));
            bugSwarm.emplace_back(std::make_unique<Bug>("Bug" + std::to_string(i)));
    }

    // Fight it out
    gameLoop(marineCorps, bugSwarm, pool);
    postProcessing(marineCorps, bugSwarm);
    
    std::cout << "Hope you enjoyed the fight! Exiting...\n";

    return 0;
}