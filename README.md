# Multithreaded chess server made in C and using the winsock API

### A quick overview:
This is the chess server that accompanies the [chess desktop application I made in C++](https://github.com/oskarGrr/MultiplayerChess). The main thread listens for new connections and places them into the lobby. From there, the lobby thread manages the players connected to the server but not yet playing a chess game. Once two players in the lobby agree to pair up, they are removed from the lobby, and a new thread starts to manage their game. When no one is in the lobby, the lobby thread does not spin in a loop with no work to do. Instead, it sleeps on a condition variable and wakes up when it has work to do again. When no one is connected to the server at all, only the main thread is running, sitting in an accept() call.

### Some future improvements:
* Building a thread safe hash table in C to look up players by their ID faster than a linear search (I have built one in C++ already, but for this I plan to build an open addressing thread safe one in C).
* Making the project cross platform. For this, I will most likely switch to a C networking library.
* Using a thread pool of game threads instead of spawning a new thread every game.
* Allowing each thread in the pool to manage multiple games (maybe 100 - 200 games) rather than being dedicated to one chess game.
* Have a thread also listen for keyboard input so you can type commands in the console.

## build inscructions
For now, the server only works on windows so I have just included a visual studio project.
It should be as simple as oppening the .sln and pressing F5.