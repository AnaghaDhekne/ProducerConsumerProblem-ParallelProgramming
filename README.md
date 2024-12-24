# Producer-Consumer Problem Simulation

## Project Overview

This project simulates a producer-consumer problem set in a plant where **Part Workers** produce various types of parts and **Product Workers** assemble products from these parts. The implementation is designed to improve plant performance while ensuring fair treatment to all workers.

Key highlights of the simulation include:
- **Concurrency:** Both part workers and product workers operate as threads.
- **Shared Resource Protection:** The buffer, used to store parts, is a shared resource protected by locks/mutexes to ensure thread safety.
- **Timeout Events:** Both part workers and product workers have maximum wait times, after which they abort their operation and regenerate new load or pickup orders.
- **Performance Metrics:** Logs are maintained to track activities and measure performance.

---

## Features

### Part Workers
- Produce five types of parts: **A, B, C, D, E**.
- Generate random load orders (e.g., `(2,0,0,2,1)`), representing the number of each type of part produced.
- Produce parts based on predefined production times:
  - **A:** 50 µs
  - **B:** 50 µs
  - **C:** 60 µs
  - **D:** 60 µs
  - **E:** 70 µs
- Load parts into a buffer with limited capacities:
  - **Buffer capacity:** `(5, 5, 4, 3, 3)` for parts **A, B, C, D, E** respectively.
- Handle partial loading when the buffer is full, retrying or aborting if `MaxTimePart` is exceeded.
- Reuse parts from previously aborted load orders when generating new orders.

### Product Workers
- Assemble products using parts from the buffer.
- Generate random pickup orders for parts (e.g., `(1,2,2,0,0)`), requiring exactly two or three types of parts.
- Retrieve parts based on predefined move times:
  - **A:** 20 µs
  - **B:** 20 µs
  - **C:** 30 µs
  - **D:** 30 µs
  - **E:** 40 µs
- Assemble products with predefined assembly times:
  - **A:** 60 µs
  - **B:** 60 µs
  - **C:** 70 µs
  - **D:** 70 µs
  - **E:** 80 µs
- Handle partial retrieval when buffer contents are insufficient, retrying or aborting if `MaxTimeProduct` is exceeded.
- Reuse parts from previously aborted pickup orders when generating new orders.

---

## Implementation

### Buffer Management
- A shared resource storing parts of types **A, B, C, D, E**.
- Operations on the buffer (load/unload parts) are synchronized using mutexes to avoid race conditions.

### Logging
- Each thread logs its activities, including:
  - Buffer state updates
  - Load or pickup order changes
  - Timeout events
- A log file (`log.txt`) records these activities to aid in performance analysis.

### Thread Management
- **Part Workers:** `m` threads produce parts.
- **Product Workers:** `n` threads assemble products.
- Each worker performs 5 iterations.

---

## Sample Main Function

```cpp
#include <iostream>
#include <vector>
#include <thread>

const int MaxTimePart{1800}, MaxTimeProduct{2000};
const int m = 20, n = 16; // Number of Part Workers and Product Workers

int main() {
    std::vector<std::thread> PartW, ProductW;
    
    for (int i = 0; i < m; ++i) {
        PartW.emplace_back(PartWorker, i + 1);
    }
    for (int i = 0; i < n; ++i) {
        ProductW.emplace_back(ProductWorker, i + 1);
    }
    
    for (auto& worker : PartW) worker.join();
    for (auto& worker : ProductW) worker.join();
    
    std::cout << "Finish!" << std::endl;
    return 0;
}
