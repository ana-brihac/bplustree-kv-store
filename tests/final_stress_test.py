import subprocess
import time
import random
import sys

def run_stress_test(iterations=100):
    print(f"Starting Final Stress Testing ({iterations} iterations)...")
    
    for i in range(1, iterations + 1):
        print(f"\n--- Iteration {i}/{iterations} ---")
        print("Starting stress workload...")
        
        # Start the workload process
        p = subprocess.Popen(["./tests/bin/stress_workload"])
        
        # Let it run for a random small duration (between 10ms and 200ms)
        sleep_time = random.uniform(0.01, 0.2)
        print(f"Letting it run for {sleep_time:.3f} seconds...")
        time.sleep(sleep_time)
        
        # KILL IT! (SIGKILL)
        print("KILLING workload!")
        p.kill()
        p.wait() # wait for it to actually die
        
        # Now verify recovery and data integrity
        print("Verifying recovery...")
        verify_p = subprocess.Popen(["./tests/bin/verify_stress"], stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        stdout, stderr = verify_p.communicate()
        
        if verify_p.returncode != 0:
            print("FAILED!")
            print(stdout)
            print(stderr)
            print(f"FAILURE on iteration {i}!")
            sys.exit(1)
        else:
            print(stdout.strip())
            
    print("\nSUCCESS! Database survived %d random crash-and-recover cycles with ZERO data corruption or loss!" % iterations)

if __name__ == "__main__":
    run_stress_test()
