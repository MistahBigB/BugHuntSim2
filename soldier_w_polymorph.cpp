//This example demonstrates inheritance
#include <iostream>
#include <cstdlib>
#include <ctime>

class Soldier{
    public:
        int health;
        bool alive;

        // Pure virtual method
        virtual void attack() = 0;

        virtual void slay() {
            std::cout << "Critical hit!\n";
        }

    // Constructor to initialize attributes
    Soldier() : health(100), alive(true) {}

    // Virtual destructor
    virtual ~Soldier() {} 
};

class Marine : public Soldier {
    public:
        int accuracy = 7;

        void attack() override {
            std::cout << "Marine is shooting...\n";
        }

        void slay() override {
            std::cout << "Marine gets a headshot on the bug!\n";
        }
};

class Bug : public Soldier {
    public:
        int accuracy = 8;
        bool carapace = true;

        void attack() override {
            std::cout << "Bug attacks with its claws...\n";
        }

        void slay() override {
            std::cout << "Bug finds a gap in the Marine's armor!\n";
        }

        void protect() {
            if (health == 0) {
                std::cout << "Bug's carapace protected it from a killing blow!\n";
                health += 50;
                carapace = false;
            }
        }

};

int main(){
    //seed the random number generator with the current time
    srand(time(0));
    int to_hit = 0;

    // Instantiate classes with pointers
    // Change dot notation for methods to arrow
    Marine* marine = new Marine;
    Bug* bug = new Bug;

    while (marine->alive && bug->alive) {
        marine->attack();   //Relies on pure virtual method 
        to_hit = rand() % 10 + 1;
        if (to_hit > marine->accuracy){
            std::cout << "Marine hits!\n";
            if (to_hit == 10) {
                marine->slay();
                bug->health -= 100;
            } else {
                bug->health -= 50;  // Reduce health
            }

            if (bug->health <= 0 && bug->carapace == true) {
                bug->protect();
            } else if (bug->health <= 0 && bug->carapace == false) {
                bug->alive = false;
                std::cout << "Bug is dead!\n";
                break;
            }
        } else {
            std::cout << "Marine misses...\n";
        }

        bug->attack();
        to_hit = rand() % 10 + 1;
        if (to_hit > bug->accuracy){
            std::cout << "Bug hits!\n";
            if (to_hit == 10) {
                bug->slay();
                marine->health -= 100;
            } else {
                marine->health -= 50;
            }

            if (marine->health <= 0) {
                marine->alive = false;
                std::cout << "Marine is dead!\n";
                break;
            }
        } else {
            std::cout << "Bug misses...\n";
        }
        }

    // Clean up
    delete marine;
    delete bug;

    return 0;
}