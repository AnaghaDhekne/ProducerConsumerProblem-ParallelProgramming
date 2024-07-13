
#include <iostream>
#include <vector>
#include <thread>
#include <mutex>
#include <chrono>
#include <random>
#include <condition_variable>
#include <fstream>
#include <numeric>

using namespace std;

// Constants for production times, buffer capacities, move times, and assembly times for each part type
const int productionTimes[] = { 50, 50, 60, 60, 70 };
const int bufferCapacities[] = { 5, 5, 4, 3, 3 };
const int moveTimes[] = { 20, 20, 30, 30, 40 };
const int assemblyTimes[] = { 60, 60, 70, 70, 80 };
const int numPartTypes = 5;

// Global constants for maximum wait times
const int MaxTimePart{ 1800 }, MaxTimeProduct{ 8000 };

// Buffer to store the parts
vector<int> buffer(numPartTypes, 0);

// Mutex and condition variable for synchronizing access to the shared buffer
// Condition variable for signaling when buffer space is available
mutex bufferMutex1, logMutex;
condition_variable bufferCondition1, bufferCondition2;

// Output file stream for logging
ofstream logFile("log.txt");

// Function to generate a random combination of part types with a sum of 5
void generateCombination(vector<int>& combination) {

	int remainingSum = 5 - std::accumulate(combination.begin(), combination.end(), 0);

	// Use current time as a seed for randomness
	std::random_device rd;
	std::mt19937 gen(rd());

	for (int i = 0; i < 5; ++i) {

		// If there's remaining sum to distribute, update the vector
		if (remainingSum > 0) {
			// Generate a random value between 0 and the maximum allowed value
			std::uniform_int_distribution<int> distribution(0, bufferCapacities[i]);
			int randomValue = distribution(gen);

			// Ensure the generated value doesn't exceed the remaining sum
			randomValue = std::min(randomValue, remainingSum);

			// Update the vector and remaining sum
			combination[i] += randomValue;
			remainingSum -= randomValue;
		}
	}
}

void generatePickupOrder(vector<int>& pickupOrder) {

	// Determine the maximum number of non-zero elements (2 or 3)
	int maxNonZeroElements = 2 + (rand() % 2);
	int sum = 0, numNonZeroFromLocal=0;

	// Initialize a vector to store non-zero elements from localState
	vector<int> nonZeroElements;
	for (int i = 0; i < numPartTypes;i++) {
		if (pickupOrder[i] != 0) {
			sum += pickupOrder[i];
			numNonZeroFromLocal++;
		}
	}

	// Fill the remaining elements with non-zero values to make the sum exactly 5
	int remainingElements = maxNonZeroElements - numNonZeroFromLocal;
	int remainingSpace = 5 - sum;
	int i = numNonZeroFromLocal;
	while (remainingSpace > 0 && remainingElements > 0) {
		int randomValue = min(remainingSpace, 1 + rand() % remainingSpace);
		pickupOrder[i++] = randomValue;
		remainingSpace -= randomValue;
		remainingElements--;
	}

	// If the sum is not 5, adjust the last element
	int currentSum = accumulate(pickupOrder.begin(), pickupOrder.end(), 0);
	if (currentSum != 5) {
		pickupOrder[maxNonZeroElements - 1] += 5 - currentSum;
	}
}

long long getCurrentTimeMicroseconds() {
	// Get the current time point
	auto now = std::chrono::system_clock::now();

	// Convert the time point to microseconds
	auto duration = now.time_since_epoch();
	return std::chrono::duration_cast<std::chrono::microseconds>(duration).count();
}

// Part worker function
void partWorker(int workerId) {
	vector<int> loadOrder(numPartTypes, 0);
	vector<int> unloadedParts(numPartTypes, 0);

	for (int iteration = 0; iteration < 5; iteration++) {

		/***************************** GENERATION AND WAIT FOR LOAD ORDER BY PART WORKER ********************************/

		// Usage with a different seed for each run
		srand(static_cast<unsigned int>(std::time(nullptr)));

		// Generate a load order
		generateCombination(loadOrder);

		// Calculate production time for the load order
		int productionTime = 0;
		for (int i = 0; i < numPartTypes; i++) {
			productionTime += productionTimes[i] * loadOrder[i];
		}

		// Sleep for the production time
		this_thread::sleep_for(chrono::microseconds(productionTime));


		unique_lock<mutex> lock(bufferMutex1);
		{
			bool canLoad = true;
			int waitTime = 0;


			/***************************** LOG FOR "NEW LOAD ORDER" ********************************/
			{
				unique_lock<mutex> lock(logMutex);

				logFile << endl;
				logFile << "Current Time: " << getCurrentTimeMicroseconds() << " us" << endl;
				logFile << "Iteration: " << iteration + 1 << endl;
				logFile << "Part Worker Id: " << workerId << endl;
				logFile << "Status: New load Order - Before transfer to buffer" << endl;
				logFile << "Buffer State: ";
				for (int i : buffer) {
					logFile << i << " ";
				}
				logFile << endl;
				logFile << "Load Order: ";
				for (int i : loadOrder) {
					logFile << i << " ";
				}
				logFile << endl << endl;
			}

			/***************************** III. TRANSFER AND WAIT OF LOAD ORDER TO BUFFER ********************************/

			// Identify if the entire load order can be transferred to the buffer or not
			// Get the parts that cannot be loaded to the unloadedParts structure
			for (int i = 0; i < numPartTypes; i++) {
				if (buffer[i] + loadOrder[i] + unloadedParts[i] > bufferCapacities[i]) {
					unloadedParts[i] = loadOrder[i] + unloadedParts[i] - (bufferCapacities[i] - buffer[i]);
					loadOrder[i] = bufferCapacities[i] - buffer[i];
					canLoad = false;
				}
			}

			// Calculate the move time of the complete/partial load order based on buffer capacity
			int moveTime = 0;
			for (int i = 0; i < numPartTypes; i++) {
				moveTime += moveTimes[i] * loadOrder[i];
			}

			// Sleep for the move time
			this_thread::sleep_for(chrono::microseconds(moveTime));

			// Transfer the full/partial load order to the buffer
			for (int i = 0; i < numPartTypes; i++) {
				buffer[i] += loadOrder[i];
			}

			/***************************** IV. LOG UPDATED LOAD ORDER AND BUFFER ********************************/
			{
				unique_lock<mutex> lock(logMutex);
				logFile << "Current Time: " << getCurrentTimeMicroseconds() << " us" << endl;
				logFile << "Iteration: " << iteration + 1 << endl;
				logFile << "Part Worker Id: " << workerId << endl;
				logFile << "Status: New load Order - After transfer to Buffer" << endl;
				logFile << "Updated Load State: ";
				for (int i : loadOrder) {
					logFile << i << " ";
				}
				logFile << endl;
				logFile << "Updated Buffer State: ";
				for (int i : buffer) {
					logFile << i << " ";
				}
				logFile << endl << endl;
			}
			

			/***************************** V. LOG UPDATED LOAD ORDER AND BUFFER ********************************/

			// If partial load order is transferred, then wait or move the unloaded parts back
			if (!canLoad) {

				// Function to move unloaded parts back to the load order
				auto moveUnloadedPartsToLoad = [&]() {
					int transferTime = 0;
					loadOrder.assign(numPartTypes, 0);
					for (int i = 0; i < numPartTypes; i++) {
						if (unloadedParts[i] > 0) {
							loadOrder[i] += unloadedParts[i];
							transferTime += moveTimes[i] + unloadedParts[i];
							unloadedParts[i] = 0;
						}
					}
					this_thread::sleep_for(chrono::microseconds(transferTime));
					};

				chrono::system_clock::time_point endTime = chrono::system_clock::now() + chrono::microseconds(MaxTimePart);
				bool partsMovedToBuffer = false; // Flag to track if any parts were moved to the buffer
				int moveTime = 0;

				// Wait for max part time if no parts were moved to buffer
				bool timeoutOccurred = bufferCondition1.wait_until(lock, endTime, [&]() {
					bool allTypesMoved = true; // Flag to track if all part types have been moved to the buffer

					for (int i = 0; i < numPartTypes; i++) {
						
						if (unloadedParts[i] != 0 && buffer[i] + unloadedParts[i] <= bufferCapacities[i]) {
							auto bufferOperationStartTime = std::chrono::steady_clock::now();

							buffer[i] += unloadedParts[i];
							moveTime += moveTimes[i] + unloadedParts[i];
							unloadedParts[i] = 0;
							auto bufferOperationEndTime = std::chrono::steady_clock::now();
							auto bufferOperationDuration = bufferOperationEndTime - bufferOperationStartTime;

							// Add buffer operation duration to endTime
							endTime += std::chrono::duration_cast<std::chrono::system_clock::duration>(bufferOperationDuration);

						}
						else {
							allTypesMoved = false; // Set the flag to false if any part type remains to be moved
						}
					}

					if (allTypesMoved) bufferCondition2.notify_one();
					return allTypesMoved; // Buffer space is not available
				});

				if (!timeoutOccurred) {
					// MaxTimePart elapsed, move unloaded parts back to the load order
					{
						unique_lock<mutex> lock(logMutex);
						logFile << "Current Time: " << getCurrentTimeMicroseconds() << " us" << endl;
						logFile << "Iteration: " << iteration + 1 << endl;
						logFile << "Part Worker Id: " << workerId << endl;
						logFile << "Status: Wake-up Timedout" << endl;
						moveUnloadedPartsToLoad();
						logFile << "Remaining Load Order: ";
						for (int i : loadOrder) {
							logFile << i << " ";
						}
						logFile << endl;
						logFile << "Updated Buffer State: ";
						for (int i : buffer) {
							logFile << i << " ";
						}
						logFile << endl << endl;
					}
				}
				else {
					this_thread::sleep_for(chrono::microseconds(moveTime));
					{
						unique_lock<mutex> lock(logMutex);
						logFile << endl;
						logFile << "Current Time: " << getCurrentTimeMicroseconds() << " us" << endl;
						logFile << "Iteration: " << iteration + 1 << endl;
						logFile << "Part Worker Id: " << workerId << endl;
						logFile << "Status: Wake-up Notified" << endl;
						logFile << "Updated Load Order: ";
						for (int i : loadOrder) {
							logFile << i << " ";
						}
						logFile << endl;
						logFile << "Updated Buffer State: ";
						for (int i : buffer) {
							logFile << i << " ";
						}
						logFile << endl << endl;
					}
					loadOrder.assign(numPartTypes, 0);
				}
			}
			else {
				loadOrder.assign(numPartTypes, 0);
			}
			{
				unique_lock<mutex> lock(logMutex);
				// Iteration completed
				logFile << "Part Worker " << workerId << " completed iteration " << iteration + 1 << endl << endl;
			}
		}
	}
}



// Product worker function
void productWorker(int workerId) {
	vector<int> pickupOrder(numPartTypes, 0);
	vector<int> cartState(numPartTypes, 0);
	vector<int> localState(numPartTypes, 0);

	int productsAssembled = 0;

	for (int iteration = 0; iteration < 5; iteration++) {

		unique_lock<mutex> lock(bufferMutex1);
		{

			// Generate a pickup order (2 or 3 non-zero elements with a sum of 5)
			generatePickupOrder(pickupOrder);


			bool canLoad = true;

			{
				unique_lock<mutex> lock(logMutex);
				logFile << endl;
				logFile << "Current Time: " << getCurrentTimeMicroseconds() << " us" << endl;
				logFile << "Iteration: " << iteration + 1 << endl;
				logFile << "Product Worker Id: " << workerId << endl;
				logFile << "Status: New Pickup Order - Before tranfer from buffer" << endl;
				logFile << "Buffer State: ";
				for (int i : buffer) {
					logFile << i << " ";
				}
				logFile << endl;
				logFile << "Pickup Order: ";
				for (int i : pickupOrder) {
					logFile << i << " ";
				}
				logFile << endl;
				logFile << "Local State: ";
				for (int i : localState) {
					logFile << i << " ";
				}
				logFile << endl;
				logFile << "Cart Order: ";
				for (int i : cartState) {
					logFile << i << " ";
				}
				logFile << endl << endl;
			}
			

			// Identify if the entire pickup order can be transferred from the buffer or not
			// Get the parts that cannot be pickedup to the localstate structure
			for (int i = 0; i < numPartTypes; i++) {
				if (buffer[i] < pickupOrder[i]) {
					localState[i] = pickupOrder[i] - buffer[i];
					pickupOrder[i] = buffer[i];
					canLoad = false;
				}
			}

			// Calculate the move time of the complete/partial pickup order
			int moveTime = 0;
			for (int i = 0; i < numPartTypes; i++) {
				moveTime += moveTimes[i] * pickupOrder[i];
			}

			// Sleep for the move time
			this_thread::sleep_for(chrono::microseconds(moveTime));

			// Transfer the full/partial pickup order to the cart
			for (int i = 0; i < numPartTypes; i++) {
				cartState[i] += pickupOrder[i];
			}

			{
				unique_lock<mutex> lock(logMutex);
				logFile << "Current Time: " << getCurrentTimeMicroseconds() << " us" << endl;
				logFile << "Iteration: " << iteration + 1 << endl;
				logFile << "Product Worker Id: " << workerId << endl;
				logFile << "Status: New Pickup Order - After tranfer from buffer" << endl;
				logFile << "Buffer State: ";
				for (int i : buffer) {
					logFile << i << " ";
				}
				logFile << endl;
				logFile << "Pickup Order: ";
				for (int i : pickupOrder) {
					logFile << i << " ";
				}
				logFile << endl;
				logFile << "Local State: ";
				for (int i : localState) {
					logFile << i << " ";
				}
				logFile << endl;
				logFile << "Cart Order: ";
				for (int i : cartState) {
					logFile << i << " ";
				}
				logFile << endl << endl;
			}
			

			// if parts are partially moved to the cart 
			// then wait for the parts to become available in buffer
			if (!canLoad) {

				// Function to move parts from the cart back to the local area if timedout
				auto moveLocalToPickup = [&]() {
					int transferTime = 0;
					pickupOrder.assign(numPartTypes, 0);
					for (int i = 0; i < numPartTypes; i++) {
						if (localState[i] > 0) {
							pickupOrder[i] += localState[i];
							transferTime += moveTimes[i] + localState[i];
							localState[i] = 0;
						}
					}
					this_thread::sleep_for(chrono::microseconds(transferTime));
					};

				auto endTime = chrono::system_clock::now() + chrono::microseconds(MaxTimeProduct);
				bool partsMovedToCart = false; // Flag to track if any parts were moved to the buffer
				int moveTime = 0;

				// Wait for max product time if no parts were moved to cart
				bool timeoutOccurred = bufferCondition2.wait_until(lock, endTime, [&]() {
					bool allTypesMoved = true; // Flag to track if all part types have been moved to the cart

					for (int i = 0; i < numPartTypes; i++) {
						if (localState[i] != 0 && buffer[i]!=0) {
							auto bufferOperationStartTime = std::chrono::steady_clock::now();
							if (buffer[i] >= localState[i]) {
								buffer[i] -= localState[i];
								cartState[i] += localState[i];
								moveTime += moveTimes[i] + localState[i];
								localState[i] = 0;
							}
							else {
								cartState[i] += buffer[i];
								localState[i] -= buffer[i];
								moveTime += moveTimes[i] + buffer[i];
								buffer[i] = 0;
								allTypesMoved = false;
							}
							auto bufferOperationEndTime = std::chrono::steady_clock::now();
							auto bufferOperationDuration = bufferOperationEndTime - bufferOperationStartTime;

							// Add buffer operation duration to endTime
							endTime += std::chrono::duration_cast<std::chrono::system_clock::duration>(bufferOperationDuration);

						}
						else {
							allTypesMoved = false; // Set the flag to false if any part type remains to be moved
						}
					}

					if (allTypesMoved) bufferCondition1.notify_one();
					return allTypesMoved; // Buffer space is not available
					});

				if (!timeoutOccurred) {
					{
						unique_lock<mutex> lock(logMutex);
						moveLocalToPickup();
						logFile << "Current Time: " << getCurrentTimeMicroseconds() << " us" << endl;
						logFile << "Iteration: " << iteration + 1 << endl;
						logFile << "Product Worker Id: " << workerId << endl;
						logFile << "Status: Wakeup timedout" << endl;
						logFile << "Buffer State: ";
						for (int i : buffer) {
							logFile << i << " ";
						}
						logFile << endl;
						logFile << "Pickup Order: ";
						for (int i : pickupOrder) {
							logFile << i << " ";
						}
						logFile << endl;
						logFile << "Local State: ";
						for (int i : localState) {
							logFile << i << " ";
						}
						logFile << endl;
						logFile << "Cart Order: ";
						for (int i : cartState) {
							logFile << i << " ";
						}
						logFile << endl << endl;
					}
				}
				else {

					{
						unique_lock<mutex> lock(logMutex);
						logFile << "Current Time: " << getCurrentTimeMicroseconds() << " us" << endl;
						logFile << "Iteration: " << iteration + 1 << endl;
						logFile << "Product Worker Id: " << workerId << endl;
						logFile << "Status: Wakeup notified" << endl;
						logFile << "Buffer State: ";
						for (int i : buffer) {
							logFile << i << " ";
						}
						logFile << endl;
						logFile << "Pickup Order: ";
						for (int i : pickupOrder) {
							logFile << i << " ";
						}
						logFile << endl;
						logFile << "Local State: ";
						for (int i : localState) {
							logFile << i << " ";
						}
						logFile << endl;
						logFile << "Cart Order: ";
						for (int i : cartState) {
							logFile << i << " ";
						}
						logFile << endl << endl;
					}

					
					this_thread::sleep_for(chrono::microseconds(moveTime));

					// Calculate the assembly time for all parts present on the cart
					int assemblyTime = 0;
					for (int i = 0; i < numPartTypes; i++) {
						assemblyTime += assemblyTimes[i] * cartState[i];
					}

					// Sleep for the assembly time
					this_thread::sleep_for(chrono::microseconds(assemblyTime));

					pickupOrder.assign(numPartTypes, 0);
				}

			}
			else {

				// Calculate the assembly time for all parts present on the cart
				int assemblyTime = 0;
				for (int i = 0; i < numPartTypes; i++) {
					assemblyTime += assemblyTimes[i] * cartState[i];
				}

				// Sleep for the assembly time
				this_thread::sleep_for(chrono::microseconds(assemblyTime));

				pickupOrder.assign(numPartTypes, 0);
			}
			{
				unique_lock<mutex> lock(logMutex);
				logFile << "Product Worker " << workerId << " completed iteration " << iteration + 1 << endl << endl;
			}

		
		}
	}
}

int main() {
	const int m = 1, n = 1; // m: number of Part Workers, n: number of Product Workers
	vector<thread> partWorkers, productWorkers;

	// Create part worker threads
	for (int i = 0; i < m; ++i) {
		partWorkers.emplace_back(partWorker, i + 1);
	}

	// Create product worker threads
	for (int i = 0; i < n; ++i) {
		productWorkers.emplace_back(productWorker, i + 1);
	}

	// Wait for all worker threads to finish
	for (auto& worker : partWorkers) {
		worker.join();
	}

	// Wait for all worker threads to finish
	for (auto& worker : productWorkers) {
		worker.join();
	}

	logFile << "Finish!" << endl;
	logFile.close(); // Close the log file
	return 0;
}
