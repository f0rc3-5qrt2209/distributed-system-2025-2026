import socket
import struct
import threading
import time
import random
import statistics
import sys

SERVER_IP = "127.0.0.1"
SERVER_PORT = 8888
NUM_CLIENTS = 50
TEST_DURATION = 30
COMMAND = "ls -la /tmp"

class LoadTestStats:
    def __init__(self):
        self.lock = threading.Lock()
        self.total_requests = 0
        self.successful_requests = 0
        self.failed_requests = 0
        self.total_bytes_received = 0
        self.latencies = []
        self.errors = []

    def record_success(self, latency_ms, bytes_received):
        with self.lock:
            self.successful_requests += 1
            self.total_requests += 1
            self.total_bytes_received += bytes_received
            self.latencies.append(latency_ms)

    def record_failure(self, error_msg):
        with self.lock:
            self.failed_requests += 1
            self.total_requests += 1
            self.errors.append(error_msg)

def send_message(sock, data):
    try:
        msg = struct.pack('!I', len(data)) + data.encode()
        sock.sendall(msg)
        return True
    except Exception:
        return False

def receive_message(sock):
    try:
        raw_len = recv_all(sock, 4)
        if not raw_len: return None
        msglen = struct.unpack('!I', raw_len)[0]
        
        data = recv_all(sock, msglen)
        return data
    except Exception:
        return None

def recv_all(sock, n):
    data = b''
    while len(data) < n:
        packet = sock.recv(n - len(data))
        if not packet:
            return None
        data += packet
    return data

def client_worker(stats, stop_event):
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.settimeout(5)
    
    try:
        s.connect((SERVER_IP, SERVER_PORT))
    except Exception as e:
        stats.record_failure(f"Connect Error: {str(e)}")
        return

    while not stop_event.is_set():
        start_time = time.time()
        
        if not send_message(s, COMMAND):
            stats.record_failure("Send Error")
            break
            
        response = receive_message(s)
        
        end_time = time.time()
        
        if response is not None:
            latency = (end_time - start_time) * 1000 
            stats.record_success(latency, len(response))
        else:
            stats.record_failure("Receive Error / Server Disconnect")
            break
        
        time.sleep(random.uniform(0.01, 0.05))

    s.close()

def main():
    stats = LoadTestStats()
    stop_event = threading.Event()
    threads = []

    print(f"--- STARTING REMOTE SHELL BENCHMARK ---")
    print(f"Server: {SERVER_IP}:{SERVER_PORT}")
    print(f"Concurrency: {NUM_CLIENTS} clients")
    print(f"Duration: {TEST_DURATION} seconds")
    print("-" * 50)
    print("Initializing connections...")

    for _ in range(NUM_CLIENTS):
        t = threading.Thread(target=client_worker, args=(stats, stop_event))
        t.daemon = True
        threads.append(t)
        t.start()

    try:
        for i in range(TEST_DURATION):
            time.sleep(1)
            sys.stdout.write(f"\rRunning... {i+1}/{TEST_DURATION}s")
            sys.stdout.flush()
    except KeyboardInterrupt:
        print("\nTest cancelled by user.")

    print("\nStopping clients...")
    stop_event.set()

    for t in threads:
        t.join(timeout=1)

    print("\n" + "="*50)
    print("BENCHMARK REPORT")
    print("="*50)

    if stats.total_requests == 0:
        print("No requests completed. Check server connection.")
        return

    duration_actual = TEST_DURATION
    throughput = stats.successful_requests / duration_actual
    error_rate = (stats.failed_requests / stats.total_requests) * 100
    
    total_mb = stats.total_bytes_received / (1024 * 1024)
    bandwidth_mbps = total_mb / duration_actual

    if stats.latencies:
        avg_lat = statistics.mean(stats.latencies)
        max_lat = max(stats.latencies)
        min_lat = min(stats.latencies)
        median_lat = statistics.median(stats.latencies)
        p99_lat = sorted(stats.latencies)[int(len(stats.latencies) * 0.99)] if len(stats.latencies) > 100 else max_lat
    else:
        avg_lat = max_lat = min_lat = median_lat = p99_lat = 0

    print(f"1. THROUGHPUT:")
    print(f"   - Total Requests: {stats.total_requests}")
    print(f"   - Successful: {stats.successful_requests}")
    print(f"   - Requests Per Second (RPS): {throughput:.2f} req/s")
    print("-" * 30)

    print(f"2. LATENCY:")
    print(f"   - Average: {avg_lat:.2f} ms")
    print(f"   - Min:     {min_lat:.2f} ms")
    print(f"   - Max:     {max_lat:.2f} ms")
    print(f"   - Median:  {median_lat:.2f} ms")
    print(f"   - P99:     {p99_lat:.2f} ms")
    print("-" * 30)

    print(f"3. RELIABILITY:")
    print(f"   - Failed Requests: {stats.failed_requests}")
    print(f"   - Error Rate: {error_rate:.2f}%")
    if stats.errors:
        print(f"   - Sample Error: {stats.errors[0]}")
    print("-" * 30)

    print(f"4. BANDWIDTH:")
    print(f"   - Total Data Received: {total_mb:.2f} MB")
    print(f"   - Application Bandwidth: {bandwidth_mbps:.2f} MB/s")
    print("="*50)

if __name__ == "__main__":
    main()