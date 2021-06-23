#include <iostream>
#include <chrono>
#include <thread>
#include <cstdlib>
#include <deque>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <ncurses.h>

using namespace std;

static mutex mut;
static condition_variable cv;

class BusStop;
class Bus;
class Person;

class BusStop
{
public:
    Bus *currentBus = nullptr;
    deque<Person *> waitingForTicket;
    deque<Person *> waitingForBus;
    bool running = true;

    BusStop();

    void setNewBus(Bus *bus);

    void run();
};

class Bus
{
public:
    int currentCapacity;
    int maxCapacity;
    bool hasController;
    bool onBusStop;
    bool departed;
    BusStop *busStop;

    Bus(int capacity, int chanceToHaveController, BusStop *stop);

    bool canBoard();

    void run();
};

class Person
{
public:
    BusStop *busStop;
    bool honest; // osoba kupi bilet jezeli jest uczciwa
    bool readyToBoard;
    bool boarded;

    Person(int chanceToBeDishonest, BusStop *stop);

    void run();
};

BusStop::BusStop()
{
}

void BusStop::setNewBus(Bus *bus)
{
    currentBus = bus;
}

void BusStop::run()
{
    unique_lock<mutex> lock(mut);
    while (running)
    {
        clear();
        if (currentBus != nullptr)
        {
            if (currentBus->onBusStop)
            {
                if (currentBus->hasController)
                {
                    mvprintw(0, 50, "BUS (z kontrolerem)");
                }
                else
                {
                    mvprintw(0, 50, "BUS");
                }
                mvprintw(1, 49, "%d/%d", currentBus->currentCapacity, currentBus->maxCapacity);
            }
            else if (currentBus->departed)
            {
                mvprintw(0, 100, "BUS (odjechal)");
            }
            else
            {
                mvprintw(0, 0, "BUS (jedzie na przystanek)");
            }
        }
        mvprintw(2, 46, "PRZYSTANEK");
        int dishonest = 0;
        for (auto person : waitingForBus)
        {
            if (!person->honest)
            {
                dishonest++;
            }
        }
        mvprintw(3, 40, "%d czekajacych na autobus, w tym %d oszustow", waitingForBus.size(), dishonest);
        mvprintw(4, 40, "%d kupujacych bilet", waitingForTicket.size());

        refresh();
        lock.unlock();
        this_thread::sleep_for(chrono::milliseconds(100));
        lock.lock();
    }
}

Bus::Bus(int capacity, int chanceToHaveController, BusStop *stop)
{
    busStop = stop;
    currentCapacity = 0;
    maxCapacity = capacity;
    onBusStop = false;
    departed = false;

    int chance = rand() % 100;
    if (chance < chanceToHaveController)
        hasController = true;
    else
        hasController = false;
}

bool Bus::canBoard()
{
    return onBusStop && currentCapacity < maxCapacity;
}

void Bus::run()
{
    unique_lock<mutex> lock(mut);
    // jedzie na przystanek
    lock.unlock();
    this_thread::sleep_for(chrono::milliseconds(2000));
    lock.lock();
    // narysuj autobus na przystanku
    onBusStop = true; // dojechał
    cv.notify_all();
    // na przystanku czeka az wszyscy wsiada, lub autobus bedzie pelny,
    cv.wait(lock, [&]
            { return currentCapacity >= maxCapacity || busStop->waitingForBus.size() == 0; });

    // odjezdza
    onBusStop = false;
    departed = true;
}

Person::Person(int chanceToBeDishonest, BusStop *stop)
{
    busStop = stop;
    readyToBoard = false;
    boarded = false;

    int chance = rand() % 100;
    if (chance < chanceToBeDishonest)
        honest = false;
    else
        honest = true;
}

void Person::run()
{
    unique_lock<mutex> lock(mut);
    if (honest)
    {
        // stoi w kolejce po bilet jesli uczciwy
        busStop->waitingForTicket.push_back(this);
        // czeka na bycie pierwszym w kolejce
        cv.wait(lock, [&]
                { return busStop->waitingForTicket[0] == this; });
        // odblokowujemy tymczasowo lock,
        // aby nie blokowowal calego programu podczas sleepa
        lock.unlock();
        // kupuje bilet jezeli jest uczciwy (trwa 0.5s)
        this_thread::sleep_for(chrono::milliseconds(500));
        lock.lock();
        busStop->waitingForTicket.pop_front();
        cv.notify_all();
    }
    // czeka na autobus w kolejce
    busStop->waitingForBus.push_back(this);
    // czekanie na mozliwosc wejscia do autobusu
    cv.wait(lock, [&]
            { return busStop->currentBus->canBoard() &&
                     busStop->waitingForBus[0] == this; });
                     
    if (busStop->currentBus->hasController && !honest)
    {
        // nie wpuszczaj i usun sie z kolejki
        busStop->waitingForBus.pop_front();
        cv.notify_all();
    }
    else
    {
        // wpusc do autobusu
        // aby nie blokowowal calego programu podczas sleepa
        lock.unlock();
        // wsiada do autobusu (trwa 0.6s)
        this_thread::sleep_for(chrono::milliseconds(600));
        lock.lock();
        // usun sie z kolejki
        busStop->waitingForBus.pop_front();
        // zwieksz liczbe osob w autobusie o 1
        busStop->currentBus->currentCapacity++;
        cv.notify_all();
    }
}

void startCurses()
{
    setlocale(LC_ALL, "");

    initscr();
    refresh();

    cbreak();
    noecho();

    // chowa kursor
    curs_set(0);
}

void endCurses()
{
    endwin();
}

int main()
{
    startCurses();

    vector<thread> threads;
    BusStop stop;
    Bus decoyBus(0, 0, &stop);
    decoyBus.departed = true;
    stop.setNewBus(&decoyBus);
    threads.emplace_back([&]
                         { stop.run(); });
    char ch;
    while ((ch = getch()) != 'q')
    {
        if (ch == 'p')
        {
            threads.emplace_back([&]
                                 { (new Person(5, &stop))->run(); });
        }
        else if (ch == 'b')
        {
            unique_lock<mutex> lock(mut);
            if (stop.currentBus->departed)
            {
                auto bus = new Bus(50, 10, &stop);
                stop.setNewBus(bus);
                threads.emplace_back([&, bus]
                                     { bus->run(); });
            }
        }
    }

    stop.running = false;

    for (auto &t : threads)
    {
        t.join();
    }

    endCurses();
    return 0;
}