#include <iostream>
#include <vector>
#include <mutex>
#include <thread>
#include <random>
#include <chrono>

using namespace std;

class Philosopher {
    int number, howMuchAte, eatTime, thinkTime;
    bool hasLeftFork, hasRightFork, finished;
public:
    Philosopher(int i, int eatTime, int thinkTime) {
        number = i;
        howMuchAte = 0;
        this->eatTime = eatTime;
        this->thinkTime = thinkTime;
        hasLeftFork = false;
        hasRightFork = false;
        finished = false;
    }

    void eatAndThink(mutex &leftFork, mutex &rightFork) {
        while (!finished) {
            leftFork.lock();
            hasLeftFork = true;
            rightFork.lock();
            hasRightFork = true;

            // eating
            int eatTimeIn_ms = eatTime * 1000; // milliseconds
            // https://stackoverflow.com/a/7577532
            mt19937_64 twisterEngine{random_device{}()};
            uniform_int_distribution<> eatDist{eatTimeIn_ms * 4 / 5, eatTimeIn_ms * 6 / 5};
            this_thread::sleep_for(chrono::milliseconds{eatDist(twisterEngine)});
            ++howMuchAte;

            hasLeftFork = false;
            leftFork.unlock();
            hasRightFork = false;
            rightFork.unlock();

            // thinking
            int thinkTimeIn_ms = thinkTime * 1000; // milliseconds
            uniform_int_distribution<> thinkDist{thinkTimeIn_ms * 4 / 5, thinkTimeIn_ms * 6 / 5};
            this_thread::sleep_for(chrono::milliseconds{thinkDist(twisterEngine)});
        }
    }

    void print() const {
        cout << "Philosopher " << number << " ate " << howMuchAte << " times";
        if (hasLeftFork)
            cout << ", has left fork";
        if (hasRightFork)
            cout << ", has right fork";
        cout << ".\n";
    }

    void finish() {
        finished = true;
    }
};

class Dinner {
    bool finished = false;
    int numberOfPhilosophers, eatTime, thinkTime;
    vector<mutex> forks;
    vector<Philosopher *> *philosophers = new vector<Philosopher *>();
    vector<thread> threads = vector<thread>();
public:

    Dinner() = default;

    void finish() {
        finished = true;
    }

    void showPhilosophers() {
        while (!finished) {
            for (int i = 0; i < numberOfPhilosophers; ++i) {
                philosophers->at(i)->print();
            }
            cout << "\n";
            this_thread::sleep_for(chrono::milliseconds(1000));
        }
    }

    void run() {
        cout << "Enter the number of philosophers: ";
        cin >> numberOfPhilosophers;
        cout << "Enter the eating time in seconds: ";
        cin >> eatTime;
        cout << "Enter the thinking time in seconds: ";
        cin >> thinkTime;

        forks = vector<mutex>(numberOfPhilosophers);
        for (int i = 0; i < numberOfPhilosophers; ++i) {
            philosophers->push_back(new Philosopher(i, eatTime, thinkTime));
        }
        for (int i = 0; i < numberOfPhilosophers - 1; ++i) {
            threads.emplace_back(
                    &Philosopher::eatAndThink,
                    philosophers->at(i),
                    // https://www.youtube.com/watch?v=_ruovgwXyYs
                    ref(forks[i]),
                    ref(forks[i + 1])
            );
        }
        threads.emplace_back(
                &Philosopher::eatAndThink,
                philosophers->at(numberOfPhilosophers - 1),
                // https://www.youtube.com/watch?v=_ruovgwXyYs
                ref(forks[0]),
                ref(forks[numberOfPhilosophers - 1])
        );
//        for (int i = 0; i < numberOfPhilosophers; ++i) {
//            threads.emplace_back(
//                    &Philosopher::eatAndThink,
//                    philosophers->at(i),
//                    // https://www.youtube.com/watch?v=_ruovgwXyYs
//                    ref(forks[min(i, (i + 1) % numberOfPhilosophers)]),
//                    ref(forks[max(i, (i + 1) % numberOfPhilosophers)])
//            );
//        }
        thread printing = thread(&Dinner::showPhilosophers, this);

        char end = 'a';
        while (end != 'q') {
            cin >> end;
        }
        finish();

        printing.join();
        for (int i = 0; i < numberOfPhilosophers; ++i) {
            philosophers->at(i)->finish();
        }
        for (int i = 0; i < numberOfPhilosophers; ++i) {
            threads[i].join();
        }
    }

};

int main() {
    auto *dinner = new Dinner();
    dinner->run();
    return 0;
}
