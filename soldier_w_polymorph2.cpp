//This example demonstrates polymorphism by having the attack() method automaticaly adapting to the attacker type.
// Additionally, takedamage() method ensures unique logic is handled within the class.
// Regarding encapsulation, the soldier class and its derived classes are responsible for managing health and life status.
#include <iostream>
#include <cstdlib>
#include <ctime>

class Soldier{
    public:
        int health;
        bool alive;

        // Constructor to initialize attributes
        // Needs to be initialized BEFORE functions pass it as an arg
        Soldier() : health(100), alive(true) {}
        
        // Pure virtual method
        virtual void attack(Soldier* target) = 0;

        virtual void slay(Soldier* target) {
            std::cout << "Critical hit!\n";
        }

        virtual void takeDamage(int damage) {
            health -= damage;
            if (health <= 0) {
                alive = false;
                std::cout << "The soldier has fallen!\n";
            }
        }

    // Virtual destructor
    virtual ~Soldier() {} 
};

class Marine : public Soldier {
    public:
        int accuracy = 7;

        void attack(Soldier* target) override {
            std::cout << "Marine is shooting...\n";
            int to_hit = rand() % 10 + 1;
            if (to_hit > accuracy){
                std::cout << "Marine hits!\n";
                if (to_hit == 10) {
                    slay(target);
                } else {
                    target->takeDamage(50);     // Standard hit
                }
            } else {
                std::cout << "Marine misses...\n";
            }
        }

        void slay(Soldier* target) override {
            std::cout << "Marine gets a headshot on the bug!\n";
            target->takeDamage(100);    // Critical hit
        }
};

class Bug : public Soldier {
    public:
        int accuracy = 8;
        bool carapace = true;

        void attack(Soldier* target) override {
            std::cout << "Bug attacks with its claws...\n";
            int to_hit = rand() % 10 + 1;
            if (to_hit > accuracy) {
                std::cout << "Bug hits!\n";
                if (to_hit == 10) {
                    slay(target);
                } else {
                    target->health -= 50;
                }
            } else {
            std::cout << "Bug misses...\n";
            }
        }

        void slay(Soldier* target) override {
            std::cout << "Bug finds a gap in the Marine's armor!\n";
            target->health -= 100;
        }
        void takeDamage(int damage) override {
            health -= damage;
            if (health <= 0 && carapace) {
                std::cout << "Bug's carapace protected it from a killing blow!\n";
                health = 50; // Restore some health
                carapace = false;
            } else if (health <= 0) {
                alive = false;
                std::cout << "The bug has fallen!\n";
            }
        }

};

int main(){
    //seed the random number generator with the current time
    srand(time(0));

    // Instantiate soldier classes with pointers
    //      Change dot notation for methods to arrow
    Marine* marine = new Marine;
    Bug* bug = new Bug;

    // Battle loop
    while (marine->alive && bug->alive) {
        marine->attack(bug);   //Relies on pure virtual method 
            if (!bug->alive) break;
        bug->attack(marine); 
            if (!marine->alive) break;
    }

    // Clean up
    delete marine;
    delete bug;

    return 0;
}