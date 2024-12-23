// This example demonstrates multithreading for combat.
// This version uses a thread for every soldier on the battlefield.

// TODO: remove dead soldiers (mark them inactive) so that they don't keep fighting after
// their health is reduced to zero.

#include <iostream>
#include <vector>
#include <thread>
#include <future>
#include <mutex>
#include <atomic>
#include <cstdlib>
#include <ctime>

class Soldier{
    public:
        int health;
        int base_to_hit = 10;

        // With atomic r/w, a thread always sees a consisent state of the var. Does not require locks.
        std::atomic<bool> alive;

        // Constructor to initialize attributes
        // Needs to be initialized BEFORE functions pass it as an arg
        Soldier() : health(100), alive(true) {}
        
        // Pure virtual method as each soldier will have a different attack
        virtual void attack(Soldier* target) = 0;

        // Default one-shot
        virtual void slay(Soldier* target) {
            int damage = 100;  // Default damage
            target->takeDamage(damage);
            std::cout << "Critical hit! " << typeid(*target).name() << " takes " << damage << " damage.\n";
        }

        virtual void takeDamage(int damage) {
            health -= damage;
            if (health <= 0) {
                alive = false;
                std::cout << typeid(*this).name() << " has been killed.\n";
            }
        }

    // Virtual destructor
    virtual ~Soldier() {} 
};

class Marine : public Soldier {
    public:
        int accuracy = 7;
        int damage = 50;
        int marine_hit = 0;

        // Modifying the standard attack
        void attack(Soldier* target) override {
            std::cout << "Marine is shooting...\n";
            int to_hit = rand() % 10 + 1;

            if (to_hit > accuracy){
                std::cout << "Marine hits!\n";
                marine_hit++;
                if (to_hit == base_to_hit) {
                    slay(target);
                } else {
                    target->takeDamage(damage);     // Standard hit
                }
            } else {
                std::cout << "Marine misses...\n";
            }
        }

        void slay(Soldier* target) override {
            std::cout << "Marine scores a headshot!\n";
            target->takeDamage(damage*2);    // Critical hit
        }
};

class Bug : public Soldier {
    public:
        int accuracy = 8;
        int damage = 50;
        bool carapace = true;
        int bug_hit = 0;

        void attack(Soldier* target) override {
            std::cout << "Bug attacks with its claws...\n";
            int to_hit = rand() % 10 + 1;

            if (to_hit > accuracy) {
                std::cout << "Bug hits!\n";
                bug_hit++;
                if (to_hit == base_to_hit) {
                    slay(target);
                } else {
                    target->takeDamage(damage);
                }
            } else {
            std::cout << "Bug misses...\n";
            }
        }

        void slay(Soldier* target) override {
            std::cout << "Bug finds a gap in the Marine's armor!\n";
            target->health -= (damage*2);
        }
        void takeDamage(int damage) override {
            health -= damage;
            if (health <= 0 && carapace) {
                std::cout << "Bug's carapace protected it from a killing blow!\n";
                health += 50; // Restore some health
                carapace = false;
            } else if (health <= 0) {
                alive = false;
                std::cout << "The bug has fallen!\n";
            }
        }
};

std::pair<int, int> gameLoop(std::vector<std::unique_ptr<Marine>>& marineForce, std::vector<std::unique_ptr<Bug>>& bugForce) {
    std::atomic<bool> gameOver = false;
    size_t marineCount = marineForce.size();
    size_t bugCount = bugForce.size();
    int sleep_time = 100;
    int total_marine_hits = 0;
    int total_bug_hits = 0;

    std::cout << "This fight is between " << marineCount << " Marines and " << bugCount << " Bugs!\n"; 

    // Combat lambda for turn based combat 
    auto battle = [&gameOver, &marineCount, &bugCount](Soldier* attacker, Soldier* defender, bool isMarineAttacking) {
        if(!attacker->alive || !defender->alive) return;

        attacker->attack(defender);

        if (!defender->alive) {
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
    };
    
    while (!gameOver) {
        std::vector<std::thread> threads;

        // Marines attack Bugs
        for (auto& marine : marineForce) {
            // Create a new thread and add it to the threads vector
            // emplace_back() directly constructs the thread obj in the vector without creating and copying a temp thread obj
            // [&] captures variables from the surrounding sopr by reference (can access gameOver, bugForce, and battle())
            threads.emplace_back([&]() {
                while (!gameOver) {
                    // Choose a random target from the opposing team
                    Soldier* target = bugForce[rand() % bugForce.size()].get();

                    // Calls battle() with the pointer to the Marine, the target, and true for isMarineAttacking
                    battle(marine.get(), target, true);
                    std::this_thread::sleep_for(std::chrono::milliseconds(sleep_time));
                }
            });
        }

        // Bugs attack Marines
        for (auto& bug : bugForce) {
            threads.emplace_back([&]() {
                while (!gameOver) {
                    Soldier* target = marineForce[rand() % marineForce.size()].get();

                    battle(bug.get(), target, false);
                    std::this_thread::sleep_for(std::chrono::milliseconds(sleep_time));
                }
            }); 
        }

        for (auto& thread : threads) {
            thread.join();
        }
    
        if (gameOver) break;
    }

    if (marineCount > 0) {
        std::cout << "Marine victory!\n";
    } else {
        std::cout << "Bugs triumphant!\n";
    }

    // Don't want to modify the object (soldier) we're referencing, so this is a const
    // Also better to reinforce std::unique_ptr is a non-copyable element
    for (const auto& marine : marineForce) {
        total_marine_hits += marine->marine_hit;
    }
    
    for (const auto& bug : bugForce) {
        total_bug_hits += bug->bug_hit;
    }

    return {total_marine_hits, total_bug_hits};
}

void postProcessing(int total_marine_hits, int total_bug_hits) {
    std::cout << "Post Fight Stats!\n";
    std::cout << "Total Marine hits this fight: " << total_marine_hits << ".\n";
    std::cout << "Total Bug hits this fight: " << total_bug_hits << ".\n";

    if (total_marine_hits > total_bug_hits) {
        std::cout << "The Marines had superior accuracy this battle!\n";
    } else if (total_bug_hits > total_marine_hits) {
        std::cout << "The Bugs were more effective in landing hits!\n";
    } else {
        std::cout << "It's a draw in terms of hit counts!\n";
    }
}

int main() {
    int marine_num = 0;
    int bug_num = 0;

    //seed the random number generator with the current time
    srand(time(0));

    // Create vectors to hold different soldier types
    std::vector<std::unique_ptr<Marine>> marineForce;
    std::vector<std::unique_ptr<Bug>> bugForce;

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

    for (int i = 0; i < marine_num; i++) {
            marineForce.emplace_back(std::make_unique<Marine>());
    }

    for (int i = 0; i < bug_num; i++) {
            bugForce.emplace_back(std::make_unique<Bug>());
    }

    // Fight it out
    auto [total_marine_hits, total_bug_hits] = gameLoop(marineForce, bugForce);
    postProcessing(total_marine_hits, total_bug_hits);
    
    std::cout << "Hope you enjoyed the fight! Exiting...\n";

    return 0;
}