# Project-1-

# Bridge Simulation

## What is this?

This program simulates traffic on a narrow one-lane bridge that connects a West side and an East side of a city. Because the bridge only fits one car at a time in terms of direction meaning all cars on it must be going the same way at any given moment someone or something needs to manage who gets to cross and when.

The simulation creates cars and ambulances that arrive on both sides of the bridge at random intervals and tries to get them all across as fairly and efficiently as possible. Every vehicle in the simulation: cars, ambulances, traffic lights, and the officer — runs as its own independent process, all happening at the same time.

There are three different ways to manage the bridge, and you pick which one you want when you run the program.


## The Three Modes

**Mode 1 — Carnage (FIFO)**
No one is in charge. Vehicles arrive and cross whenever the bridge is going their direction or is empty. If the bridge is busy going the other way, you wait. First come, first served the first vehicle to show up goes first. Ambulances only get special treatment when they are right at the entrance of the bridge; at that point they are let through ahead of regular cars.

**Mode 2 — Traffic Lights**
There are two traffic lights, one on each side. Each light has its own timer and alternates between green and red on its own the lights never stop or pause for any reason. When your light is green you can cross, when it's red you wait. Again, ambulances that reach the front of the line get special treatment: if an ambulance is right at the entrance, it can cross even on a red light as long as the bridge is clear in its direction.

**Mode 3 — Traffic Officer**
A traffic officer manually controls who crosses. The officer lets a set number of vehicles go one direction (K1), then switches and lets another set go the other direction (K2), and repeats. If one side runs out of vehicles before the batch is done, the officer switches early to avoid wasting time. Ambulances at the front of the line are handled with priority they can cross even when the count has run out.

---

## How to Run

First, make sure you have a config file ready (see the section below). Then open a terminal in the project folder and run:

Install dependencies (once) for using the graphical interface
```
make install-deps
```

```
make
./bridge <test> <mode>
```
Replace `<test>` with the with the type of test you want

`<test1>`
`<test2>`
`<test3>`

Replace `<mode>` with the number of the mode you want:

```
./bridge test1 1    # Carnage (FIFO)
./bridge test2 2    # Traffic Lights
./bridge test3 3    # Traffic Officer
```

The simulation will print everything that happens to the terminal in real time who is waiting, who is crossing, who just made it to the other side, and the state of the bridge after each crossing.

---

## The Configuration File (test 1, 2 and 3)

The config file controls everything about the simulation. It uses a simple format:

```
parameter_name = value
```

Lines that start with `#` are comments and are ignored. All parameters are required the program will tell you clearly if something is missing or invalid.

Here is what each parameter means:

---

### Bridge

Parameter | What it controls

`bridge_length`:How long the bridge is, in meters. The bridge is divided into slots of 4.5 m (one per car), so a 45 m bridge fits 10 cars at once. 

---

### West Side — vehicles travelling West → East

Parameter | What it controls 

`west_arrival_mean`:How often a new vehicle shows up on the West side, in seconds on average. A value of 2.0 means a new car arrives roughly every 2 seconds. Lower = more traffic. 
`west_speed_min`:The slowest a West-side vehicle can travel, in meters per second. 
`west_speed_max`:The fastest a West-side vehicle can travel, in meters per second. Each vehicle gets a random speed between these two values. Ambulances always travel at the maximum speed. 

---

### East Side — vehicles travelling East → West

Parameter | What it controls 

`east_arrival_mean`: How often a new vehicle shows up on the East side, in seconds on average. 
`east_speed_min`: The slowest an East-side vehicle can travel, in meters per second. 
`east_speed_max`: The fastest an East-side vehicle can travel, in meters per second. 

---

### Mode 2 — Traffic Lights

Parameter | What it controls 

`green_duration`: How many seconds each side stays green before switching to red. Both sides use this same duration. 

---

### Mode 3 — Traffic Officer

Parameter | What it controls 

`k1`:How many vehicles the officer lets through West → East in one batch before switching. 
`k2`:How many vehicles the officer lets through East → West in one batch before switching. 

---

### General

Parameter | What it controls

`ambulance_pct`:The fraction of all vehicles that are ambulances. Use a value between 0.0 and 1.0. For example, `0.10` means 10% of vehicles will be ambulances.
`simulation_duration`:How many seconds the simulation runs before stopping. 

---

### Example config file

```
bridge_length       = 45.0      # 10 slots
west_arrival_mean   = 2.0
west_speed_min      = 5.0
west_speed_max      = 15.0
east_arrival_mean   = 3.0
east_speed_min      = 4.0
east_speed_max      = 12.0
green_duration      = 10.0
k1                  = 4
k2                  = 3
ambulance_pct       = 0.10
simulation_duration = 60.0
```
