#include "hw6.h"
#include <stdio.h>
#include<pthread.h>
#include <stdlib.h>

pthread_mutex_t lock;

typedef struct elevatorInfo{
		int currPass;
		int currFloor;
		int init;
		int direc;
    int occupied;
		int numPass;

    pthread_mutex_t L;
}elevatorInfo;

typedef struct passInfo{
    int taken;
    int droppedOff;
    int end;
    int currElevator;
    int start;
		int direction;

    pthread_barrier_t passBarrier;
}passInfo;

passInfo* passArray[PASSENGERS];
elevatorInfo* ele[ELEVATORS];
pthread_barrier_t passChecked;

enum {ELEVATOR_ARRIVED=1, ELEVATOR_OPEN=2, ELEVATOR_CLOSED=3} state;

void scheduler_init()
{
	state=ELEVATOR_ARRIVED;
	pthread_mutex_init(&lock,0);

	int i = 0;

	int counter = 0;
	for(i = 0; i < PASSENGERS; i++)
  {
			passArray[i] = (passInfo*) malloc(sizeof(passInfo));
			pthread_barrier_init(&passArray[i]->passBarrier, NULL, 2);
	}
	pthread_barrier_init(&passChecked, NULL, PASSENGERS+ELEVATORS);

	for(int i = 0; i < ELEVATORS; i++)
  {
			ele[i] = (elevatorInfo*) malloc(sizeof(elevatorInfo));

      ele[i]->init = 0;
			ele[i]->direc = -1;
      ele[i]->currPass = -1;
      ele[i]->currFloor = 0;
      ele[i]->occupied = 0;

			pthread_mutex_init(&ele[i]->L, 0);
			counter++;
	}
}

void passenger_request(int passenger, int from_floor, int to_floor,
											 void (*enter)(int, int),
											 void(*exit)(int, int))
{
  // wait for the elevator to arrive at our origin floor, then get in

	  pthread_mutex_lock(&lock);
	  passArray[passenger]->taken = 0;
	  passArray[passenger]->end = to_floor;
	  passArray[passenger]->droppedOff = 0;
	  passArray[passenger]->start = from_floor;
	  pthread_mutex_unlock(&lock);

	  pthread_barrier_wait(&passChecked);

	  pthread_mutex_unlock(&lock);
	  pthread_barrier_wait(&passArray[passenger]->passBarrier);
	  pthread_mutex_lock(&lock);
	  enter(passenger, passArray[passenger]->currElevator);

	  log(6, "%i. Rider entering\n", passenger);

	  pthread_mutex_unlock(&lock);
	  pthread_barrier_wait(&passArray[passenger]->passBarrier);

	  pthread_mutex_lock(&lock);
	  exit(passenger, passArray[passenger]->currElevator);
	  pthread_mutex_unlock(&lock);
}

void elevator_ready(int elevator, int at_floor, void(*move_direction)(int, int),	void(*door_open)(int),void(*door_close)(int))
  {
    pthread_mutex_lock(&ele[elevator]->L);
      ele[elevator]->currFloor = at_floor;
			//If the elevator hasn't been initialized, unlock and intiialize
      if(ele[elevator]->init == 0)
			{
          pthread_mutex_unlock(&ele[elevator]->L);
          pthread_barrier_wait(&passChecked); //critical section
          pthread_mutex_lock(&ele[elevator]->L);
          ele[elevator]->init = 1;
      }
			//If the elevator is occupied
      if(ele[elevator]->occupied == 1)
			{
          log(6, "%i. Elevator full\n", elevator);
          if(at_floor == passArray[ele[elevator]->currPass]->end)
					{
              log(6, "%i. Moving rider\n", elevator);
              door_open(elevator);
              state=ELEVATOR_OPEN;

              pthread_mutex_unlock(&ele[elevator]->L);
              pthread_barrier_wait(&passArray[ele[elevator]->currPass]->passBarrier);
              pthread_mutex_lock(&ele[elevator]->L);

              passArray[ele[elevator]->currPass]->taken = 0;
              passArray[ele[elevator]->currPass]->droppedOff = 1;

              ele[elevator]->occupied = 0;
              ele[elevator]->currPass = -1;
							ele[elevator]->numPass -= 1;

              door_close(elevator);
              state=ELEVATOR_CLOSED;
          }
          else
					{
              log(6, "%i. Transporting rider\n", elevator);
              if(at_floor==FLOORS-1 || at_floor==0)
							{
                  ele[elevator]->direc *= -1;
							}
							else if(at_floor < passArray[ele[elevator]->currPass]->end)
							{
                  ele[elevator]->direc = 1;
							}
              else if(at_floor > passArray[ele[elevator]->currPass]->end)
							{
                  ele[elevator]->direc = -1;
							}

							log(6, "%i. Currently going in the %i direction\n", elevator, ele[elevator]->direc);
              move_direction(elevator,ele[elevator]->direc);
              state=ELEVATOR_ARRIVED;
          }
      }
			//Otherwise, the elevator is unoccupied
      else if(ele[elevator]->occupied == 0)
			{
				int flag = 0;
          for(int i = 0; i < PASSENGERS; i++)
					{
              if(passArray[i]->droppedOff == 0 && passArray[i]->taken == 0 && i % ELEVATORS == elevator && ele[elevator]->occupied == 0 && passArray[i]->start == at_floor )
							{
                  door_open(elevator);

                  state=ELEVATOR_OPEN;

                  ele[elevator]->currPass = i;
                  ele[elevator]->occupied = 1;
									ele[elevator]->numPass += 1;

                  passArray[i]->currElevator = elevator;
                  passArray[i]->taken = 1;

                  pthread_mutex_unlock(&ele[elevator]->L);
                  pthread_barrier_wait(&passArray[i]->passBarrier);
                  pthread_mutex_lock(&ele[elevator]->L);
                  door_close(elevator);

                  state=ELEVATOR_CLOSED;
                  break;
              }
          }
					//If you're at the top or bottom, you have to reverse direction
          if(at_floor==FLOORS-1 || at_floor==0)
					{
              ele[elevator]->direc*=-1;
					}
          move_direction(elevator,ele[elevator]->direc);
      }
      log(6, "%i. Moving on\n", elevator);
      pthread_mutex_unlock(&ele[elevator]->L);
    }
