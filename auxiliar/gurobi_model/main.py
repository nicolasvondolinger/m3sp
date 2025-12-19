import subprocess
import os
import glob
from model import build_and_solve
import sys

# Step 1: Run the generator
def run_generator(tipo, n_instances, n_devices, dimension):
    cmd = ["python3", "./generator.py", tipo, str(n_instances), str(n_devices), str(dimension)]
    subprocess.run(cmd, check=True)
    print("Instance generation completed.")

# Step 2: Find generated instances
def get_instance_files(folder="./"):
    # Assuming the generator puts the instances in a folder
    return sorted(glob.glob(os.path.join(folder, "*.txt")))

# Step 3: Parse instance data
def parse_instance(file_path):
    with open(file_path, 'r') as f:
        lines = [line.strip() for line in f if line.strip()]

    # Read parameters
    first_line = lines[0].split()
    n_connections = int(first_line[0])
    alpha = float(first_line[1])
    noise = float(first_line[2])
    power = float(first_line[3])
    n_spectrums = int(first_line[4])
    n_time_slots = int(first_line[5])
    spectrums = list(map(float, first_line[5:5+n_spectrums]))

    # Read positions
    sender_positions = []
    receiver_positions = []
    idx = 1
    for _ in range(n_connections):
        sender_positions.append(tuple(map(float, lines[idx].split())))
        idx += 1
    for _ in range(n_connections):
        receiver_positions.append(tuple(map(float, lines[idx].split())))
        idx += 1

    # Read gamma values
    gamma_values = []
    for _ in range(n_connections):
        gamma_values.append(float(lines[idx]))
        idx += 1

    # Read data rates (all on one line)
    rates = list(map(float, lines[idx].split()))
    idx += 1
    
    # Read SINR requirements (all on one line)
    sinrs = list(map(float, lines[idx].split()))

    # Calculate distances between senders and receivers
    d = {}
    for i in range(n_connections):
        x1, y1 = sender_positions[i]
        x2, y2 = receiver_positions[i]
        distance = ((x2-x1)**2 + (y2-y1)**2)**0.5
        d[(i+1, i+1)] = distance  # Using 1-based indexing for consistency

    # Prepare the instance data structure
    instance_data = {
        "V": list(range(1, n_connections+1)),  # Devices
        "L": [(i+1, i+1) for i in range(n_connections)],  # Links
        "T": list(range(1, n_time_slots+1)),  # Time slots 
        "C": list(range(1, n_spectrums+1)),  # Channels
        "spectrums": spectrums,  # Bandwidth spectrums
        "sinrs": sinrs,  # SINR requirements
        "rates": rates,  # Data rates
        "R": [(b, s) for b in spectrums for s in sinrs],  # Bandwidth-SINR combinations
        "lc": {c: spectrums[c-1] for c in range(1, n_spectrums+1)},  # Channel bandwidths
        "rbs": {(spectrums[b_idx], sinrs[s_idx]): rates[b_idx] 
               for b_idx in range(n_spectrums) 
               for s_idx in range(len(sinrs))},
        "O": [(c1, c2) for c1 in range(1, n_spectrums+1) 
              for c2 in range(1, n_spectrums+1) 
              if c1 != c2],  # Orthogonal channels
        "P": {(i+1, i+1): power for i in range(n_connections)},  # Transmit power
        "d": d,  # Distances
        "alpha": alpha,  # Path loss exponent
        "N": noise,  # Noise power
        "n_connections": n_connections  # Number of connections
    }
    assert len(spectrums) == n_spectrums, "Número de espectros não coincide"
    assert len(sender_positions) == n_connections, "Número de emissores incorreto"
    return instance_data

def solve_instance(instance_data):
    print(f"Solving instance with {instance_data['n_connections']} connections...")
    model = build_and_solve(instance_data)

# Step 5: Put it all together
def main():
    if len(sys.argv) != 5:
        print("Usage: python3 main.py TIPO N_INSTANCIAS N_DISPOSITIVOS DIMENSAO")
        sys.exit(1)

    tipo = sys.argv[1]
    n_instances = int(sys.argv[2])
    n_devices = int(sys.argv[3])
    dimension = int(sys.argv[4])

    # Run generator with command line arguments
    run_generator(tipo=tipo, n_instances=n_instances, n_devices=n_devices, dimension=dimension)

    # Find instances
    instance_files = get_instance_files()

    for file in instance_files:
        print(f"\n--- Processing {file} ---")
        instance_data = parse_instance(file)
        solve_instance(instance_data)

if __name__ == "__main__":
    main()