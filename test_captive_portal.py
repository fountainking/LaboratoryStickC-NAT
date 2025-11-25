#!/usr/bin/env python3
"""
Test script for captive portal multi-device socket handling.
Simulates iOS/Android captive detection with parallel connections.
"""

import requests
import concurrent.futures
import time
from typing import List, Tuple

# Captive portal IP (default AP address)
PORTAL_IP = "192.168.4.1"
BASE_URL = f"http://{PORTAL_IP}"

# Common captive portal detection endpoints
DETECTION_URLS = [
    "/",
    "/generate_204",  # Android
    "/hotspot-detect.html",  # iOS/macOS
    "/connectivity-check.html",  # Android
    "/ncsi.txt",  # Windows
]

def test_single_request(url: str, timeout: int = 5) -> Tuple[str, bool, float]:
    """Test a single request to the captive portal."""
    start = time.time()
    try:
        response = requests.get(url, timeout=timeout, allow_redirects=False)
        elapsed = time.time() - start
        success = response.status_code in [200, 302]
        return (url, success, elapsed)
    except Exception as e:
        elapsed = time.time() - start
        print(f"Error on {url}: {e}")
        return (url, False, elapsed)

def test_concurrent_connections(num_connections: int = 15):
    """Simulate multiple devices connecting simultaneously."""
    print(f"\n{'='*60}")
    print(f"Testing {num_connections} concurrent connections")
    print(f"{'='*60}\n")

    # Create a mix of requests simulating multiple devices
    urls = []
    for i in range(num_connections):
        url = BASE_URL + DETECTION_URLS[i % len(DETECTION_URLS)]
        urls.append(url)

    start_time = time.time()
    results = []

    # Execute all requests concurrently
    with concurrent.futures.ThreadPoolExecutor(max_workers=num_connections) as executor:
        futures = [executor.submit(test_single_request, url) for url in urls]
        results = [f.result() for f in concurrent.futures.as_completed(futures)]

    total_time = time.time() - start_time

    # Analyze results
    successful = sum(1 for _, success, _ in results if success)
    failed = len(results) - successful
    avg_response_time = sum(elapsed for _, _, elapsed in results) / len(results)

    print(f"Results:")
    print(f"  Total requests: {len(results)}")
    print(f"  Successful: {successful}")
    print(f"  Failed: {failed}")
    print(f"  Success rate: {(successful/len(results)*100):.1f}%")
    print(f"  Average response time: {avg_response_time:.3f}s")
    print(f"  Total test time: {total_time:.3f}s")

    if failed > 0:
        print(f"\n  ⚠️  {failed} connections failed - possible socket exhaustion!")
    else:
        print(f"\n  ✓ All connections successful!")

    return successful == len(results)

def test_sequential_connections(num_connections: int = 5):
    """Test sequential connections to verify basic functionality."""
    print(f"\n{'='*60}")
    print(f"Testing {num_connections} sequential connections")
    print(f"{'='*60}\n")

    successful = 0
    for i, endpoint in enumerate(DETECTION_URLS[:num_connections]):
        url = BASE_URL + endpoint
        _, success, elapsed = test_single_request(url)
        status = "✓" if success else "✗"
        print(f"  {status} {endpoint}: {elapsed:.3f}s")
        if success:
            successful += 1

    print(f"\nSequential test: {successful}/{num_connections} successful")
    return successful == num_connections

def main():
    print("\nCaptive Portal Multi-Device Test")
    print("=" * 60)
    print(f"Target: {BASE_URL}")
    print("Ensure M5Stick-C is running with captive portal active!")
    print()

    input("Press Enter to start tests...")

    # Test 1: Basic sequential connectivity
    seq_pass = test_sequential_connections(5)

    time.sleep(2)

    # Test 2: Light concurrent load (should work with old config)
    light_pass = test_concurrent_connections(8)

    time.sleep(2)

    # Test 3: Heavy concurrent load (tests new socket limit)
    heavy_pass = test_concurrent_connections(15)

    # Summary
    print(f"\n{'='*60}")
    print("Test Summary")
    print(f"{'='*60}")
    print(f"Sequential (5 req):     {'PASS ✓' if seq_pass else 'FAIL ✗'}")
    print(f"Light concurrent (8):   {'PASS ✓' if light_pass else 'FAIL ✗'}")
    print(f"Heavy concurrent (15):  {'PASS ✓' if heavy_pass else 'FAIL ✗'}")

    if heavy_pass:
        print("\n✓ Socket configuration supports multi-device captive detection!")
    else:
        print("\n✗ Socket exhaustion detected - check LWIP_MAX_SOCKETS config")

if __name__ == "__main__":
    main()
