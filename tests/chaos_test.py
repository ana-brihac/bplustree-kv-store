import subprocess
import time
import random
import sys
import os

ITERATIONS = 50

def run_chaos_test():
    print(f"Starting Chaos Testing ({ITERATIONS} iterations)...")
    
    # Ensure fresh state
    if os.path.exists("test_chaos.db"): os.remove("test_chaos.db")
    if os.path.exists("test_chaos.wal"): os.remove("test_chaos.wal")
    if os.path.exists("chaos_tracker.txt"): os.remove("chaos_tracker.txt")

    for i in range(ITERATIONS):
        print(f"\n--- Iteration {i+1}/{ITERATIONS} ---")
        
        # 1. Start the workload
        print("Starting workload...")
        p = subprocess.Popen(["./tests/bin/run_workload"])
        
        # 2. Sleep a random amount of time (10ms to 200ms)
        sleep_time = random.uniform(0.01, 0.2)
        print(f"Letting it run for {sleep_time:.3f} seconds...")
        time.sleep(sleep_time)
        
        # 3. Kill the workload forcefully (SIGKILL)
        print("KILLING workload!")
        p.kill()
        p.wait()
        
        # 4. Verify recovery and integrity
        print("Verifying recovery...")
        ret = subprocess.call(["./tests/bin/verify_workload"])
        
        if ret != 0:
            print(f"FAILURE on iteration {i+1}!")
            sys.exit(1)
            
    print("\nSUCCESS! Database survived 50 random crash-and-recover cycles without losing data!")

if __name__ == "__main__":
    run_chaos_test()
