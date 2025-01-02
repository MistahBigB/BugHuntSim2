// This example demonstrates turn based combat.
// This version DOES NOT require threads for every soldier on the battlefield, as each
// turn's calculations can be run by a single thread.
// Added a player request for how many marines and bugs will fight.
// Removed dead soldiers (mark them inactive) so that they don't keep fighting after
// their health is reduced to zero. Added logic to maintain their hit count afterward.

#include <iostream>
#include <vector>
#include <thread>
#include <future>
#include <mutex>
#include <atomic>
#include <cstdlib>
#include <ctime>
#include <numeric>  // For std::accumulate()
#include <condition_variable> // to syncronize threading
#include <algorithm>    // for std::none_of()

std::mutex turn_mutex;
std::condition_variable turn_cv;
bool marine_turn = true;                // Flag to control who is attacking
std::atomic<bool> game_over = false;    // Flag to control game duration

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

void marineAttack(std::vector<std::unique_ptr<Marine>>& marine_force, std::vector<std::unique_ptr<Bug>>& bug_force, std::vector<int>& marine_hits) {
    for (auto& marine : marine_force) {
        if (!marine->alive) continue;
        Soldier* target = bug_force[rand() % bug_force.size()].get();
        marine->attack(target);
        marine_hits.push_back(marine->marine_hit);  // Store individual hits
    }
}

void bugAttack(std::vector<std::unique_ptr<Bug>>& bug_force, std::vector<std::unique_ptr<Marine>>& marine_force, std::vector<int>& bug_hits) {
    for (auto& bug : bug_force) {
        if (!bug->alive) continue;
        Soldier* target = marine_force[rand() % marine_force.size()].get();
        bug->attack(target);
        bug_hits.push_back(bug->bug_hit);   // Store individual hit
    }
}

std::pair<int, int> gameLoop(std::vector<std::unique_ptr<Marine>>& marine_force, std::vector<std::unique_ptr<Bug>>& bug_force) {
    std::atomic<bool> game_over = false;
    size_t marine_count = marine_force.size();
    size_t bug_count = bug_force.size();
    int sleep_time = 100;
    int total_marine_hits = 0;
    int total_bug_hits = 0;
    int turn_number = 1;

    // Tracking dead soldiers for stats
    std::vector<int> marine_hits;  // Preserve marine hit counts
    std::vector<int> bug_hits;     // Preserve bug hit counts

    std::cout << "This fight is between " << marine_count << " Marines and " << bug_count << " Bugs!\n"; 
    std::cout << "Turn 1 begins!\n";

    // Combat lambda for turn based combat 
    auto battle = [&game_over, &marine_count, &bug_count, &marine_hits, &bug_hits](Soldier* attacker, Soldier* defender, bool is_marine_attacking) {
        if(!attacker->alive || !defender->alive) return;

        attacker->attack(defender);
        /*
        if (is_marine_attacking) {
            //dynamic_cast ensures the attacker is correctly ID'ed as a Marine or Bug
            Marine* marine = dynamic_cast<Marine*>(attacker);

            if (marine) {
                marine_hits.push_back(marine->marine_hit);
            }
        } else {
            Bug* bug = dynamic_cast<Bug*>(attacker);
            if (bug) {
                bug_hits.push_back(bug->bug_hit);
            }
        }
        */
        if (!defender->alive) {
            if (is_marine_attacking) {
                --bug_count;
                std::cout << bug_count << " bugs remain!\n";
                if (bug_count == 0) game_over = true;
            } else {                    
                --marine_count;
                std::cout << marine_count << " Marines remain!\n";
                if (marine_count == 0) game_over = true;
            }
        }
    };
    
    while (!game_over) {
        std::unique_lock<std::mutex> lock(turn_mutex);

        if (marine_turn) {
            marineAttack(marine_force, bug_force, marine_hits);
        }
        if (!marine_turn) {
            bugAttack(bug_force, marine_force, bug_hits);
        }
    
        // Check if the game is over
        // none_of checks if elements in a given range satisfy a specific condition eg soldier->alive
        // std::atomics do not permit copying, which this lambda is attempting. Thus, .load() is necessary to read the value directly.
        if (std::none_of(bug_force.begin(), bug_force.end(), [](const auto& bug) { return bug->alive.load(); })) {
            std::cout << "Marines are victorious!\n";
            game_over = true;
        } else if (std::none_of(marine_force.begin(), marine_force.end(), [](const auto& marine) { return marine->alive.load(); })) {
            std::cout << "Bugs triumph!\n";
            game_over = true;
        }

        // Toggle turn
        marine_turn = !marine_turn;
        if (marine_turn) {
            ++turn_number;
            std::cout << "\nTurn " << turn_number << " begins!\n\n";
        }
        turn_cv.notify_all();
        lock.unlock();
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // Sum hits from active Marines and Bugs
    // Don't want to modify the object (soldier) we're referencing, so this is a const
    // Also better to reinforce std::unique_ptr is a non-copyable element
    for (const auto& marine : marine_force) {
        if (marine->alive) {
            total_marine_hits += marine->marine_hit;
        }
    }
    std::cout << "\nHits from living Marines: " << total_marine_hits << "\n";
    
    for (const auto& bug : bug_force) {
        if (bug->alive) {
            total_bug_hits += bug->bug_hit;
        }
    }

    std::cout << "\nHits from living Bugs: " << total_bug_hits << "\n";

    // Add stats from dead soldiers
    //std::cout << "\nHits from dead Marines: " << std::accumulate(marine_hits.begin(), marine_hits.end(), 0) << "\n";
    //std::cout << "\nHits from dead Bugs: " << std::accumulate(bug_hits.begin(), bug_hits.end(), 0) << "\n";
    
    total_marine_hits += std::accumulate(marine_hits.begin(), marine_hits.end(), 0);
    total_bug_hits += std::accumulate(bug_hits.begin(), bug_hits.end(), 0);

    std::cout << "\nTotal Marine hits: " << total_marine_hits << "\n";
    std::cout << "\nTotal Bug hits: " << total_bug_hits << "\n";

    return {total_marine_hits, total_bug_hits};
}

void postProcessing(int total_marine_hits, int total_bug_hits) {
    std::cout << "\n\nPost Fight Stats!\n\n";
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
    int turn_max = 0;
    char first_turn;

    //seed the random number generator with the current time
    srand(time(0));

    // Create vectors to hold different soldier types
    std::vector<std::unique_ptr<Marine>> marine_force;
    std::vector<std::unique_ptr<Bug>> bug_force;

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
            marine_force.emplace_back(std::make_unique<Marine>());
    }

    for (int i = 0; i < bug_num; i++) {
            bug_force.emplace_back(std::make_unique<Bug>());
    }

    std::cout << "Which force should go first? Select Marines with m or Bugs with b.\n";
    std::cin >> first_turn;
    
    if (first_turn != 'm' && first_turn != 'b') {
        std::cout << "Invalid choice, defaulting to Marines going first.\n";
        first_turn = 'm';  // default to Marines if invalid input
    }

    marine_turn = (first_turn == 'm');

    std::cout << "How many turns should this battle go?\n";
    std::cin >> turn_max;

    // Fight it out
    auto [total_marine_hits, total_bug_hits] = gameLoop(marine_force, bug_force);
    postProcessing(total_marine_hits, total_bug_hits);
    
    std::cout << "Hope you enjoyed the fight! Exiting...\n";

    return 0;
}