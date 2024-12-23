//This example demonstrates inheritance
#include <iostream>
#include <cstdlib>
#include <ctime>

class Soldier{
    public:
        int health;
        bool alive;

    // Constructor to initialize attributes
    Soldier() : health(100), alive(true) {}  
};

class Marine : public Soldier {
    public:
        int accuracy = 7;

        void shoot() {
            std::cout << "Marine is shooting...\n";
        }
};

class Bug : public Soldier {
    public:
        int accuracy = 8;
        bool carapace = true;

        void claws() {
            std::cout << "Bug attacks with its claws...\n";
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

    //Instantiate classes
    Marine marine;
    Bug bug;

    while (marine.alive && bug.alive) {
        marine.shoot();
        to_hit = rand() % 10 + 1;
        if (to_hit > marine.accuracy){
            std::cout << "Marine hits!\n";
            bug.health -= 50;  // Reduce health
            if (bug.health <= 0 && bug.carapace == true) {
                bug.protect();
            } else if (bug.health <= 0 && bug.carapace == false) {
                bug.alive = false;
                std::cout << "Bug is dead!\n";
                break;
            }
        } else {
            std::cout << "Marine misses...\n";
        }

        bug.claws();
        to_hit = rand() % 10 + 1;
        if (to_hit > bug.accuracy){
            std::cout << "Bug hits!\n";
            if (marine.health <= 0) {
                marine.alive = false;
                std::cout << "Marine is dead!\n";
                break;
            }
        } else {
            std::cout << "Bug misses...\n";
        }
        }

    return 0;
}