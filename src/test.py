import math
import gurobipy as gp
from gurobipy import GRB
import sys
import os
model_path = os.path.abspath(os.path.join(os.path.dirname(__file__), '../auxiliar/gurobi_model'))
sys.path.append(model_path)
import model

EPS = 1e-6

dataRates = [
    [8.6, 17.2, 36.0, 72.1],
    [17.2, 34.4, 72.1, 144.1],
    [25.8, 51.6, 108.1, 216.2],
    [34.4, 68.8, 144.1, 288.2],
    [51.6, 103.2, 216.2, 432.4],
    [68.8, 137.6, 288.2, 576.5],
    [77.4, 154.9, 324.3, 648.5],
    [86.0, 172.1, 360.3, 720.6],
    [103.2, 206.5, 432.4, 864.7],
    [114.7, 229.4, 480.4, 960.8],
    [129.0, 258.1, 540.4, 1080.9],
    [143.4, 286.8, 600.5, 1201.0]
]

SINR = [
    [-5, -5, -2, -2],
    [-2, -2, 1, 1],
    [1, 1, 4, 4],
    [4, 4, 7, 7],
    [7, 7, 10, 10],
    [10, 10, 13, 13],
    [13, 13, 16, 16],
    [16, 16, 19, 19],
    [19, 19, 22, 22],
    [22, 22, 25, 25],
    [25, 25, 28, 28],
    [28, 28, 31, 31]
]

distanceMatrix = []
affectance = []
noise = 0.0
decoded_target_sol = None

class Connection:
    def __init__(self, id, interference, throughput, dist):
        self.id = id
        self.interference = interference
        self.throughput = throughput
        self.dist = dist
        self.SINR = 0.0

class Channel:
    def __init__(self, bandwidth):
        self.bandwidth = bandwidth
        self.connections = []
        self.violation = 0.0
        self.throughput = 0.0

class Spectrum:
    def __init__(self, maxFrequency, usedFrequency):
        self.maxFrequency = maxFrequency
        self.usedFrequency = usedFrequency
        self.channels = []

class Slot:
    def __init__(self):
        self.spectrums = []

class SolutionObj:
    def __init__(self, slots, throughput):
        self.slots = slots
        self.throughput = throughput

def approximatelyEqual(a, b, epsilon=EPS):
    return abs(a - b) <= (max(abs(b), abs(a)) * epsilon)

def convertBand(value):
    if value < 0.25: return 20
    elif value < 0.50: return 40
    elif value < 0.75: return 80
    return 160

def bandIndex(band):
    if band == 40: return 1
    elif band == 80: return 2
    elif band == 160: return 3
    return 0

def computeConnectionThroughput(connection, band):
    if band == 0: return 0.0
    mcs = -1

    if approximatelyEqual(connection.interference, 0.0, EPS):
        mcs = 11
        connection.throughput = dataRates[mcs][bandIndex(band)]
    else:
        connection.SINR = affectance[connection.id][connection.id] / (connection.interference + noise)
        mcs = 11
        while mcs >= 0 and connection.SINR < SINR[mcs][bandIndex(band)]:
            mcs -= 1
        
        if mcs < 0:
            connection.throughput = 0.0
        else:
            connection.throughput = dataRates[mcs][bandIndex(band)]
            
    return connection.throughput

def insertInChannel(newChannel, id):
    connectionToInsert = Connection(id, 0.0, 0.0, distanceMatrix[id][id])
    
    resChannel = Channel(newChannel.bandwidth)
    for c in newChannel.connections:
        resChannel.connections.append(Connection(c.id, c.interference, c.throughput, c.dist))

    for connection in resChannel.connections:
        connection.interference += affectance[connection.id][connectionToInsert.id]
        connectionToInsert.interference += affectance[connectionToInsert.id][connection.id]
    
    resChannel.connections.append(connectionToInsert)
    resChannel.violation = 0.0
    resChannel.throughput = 0.0

    for connection in resChannel.connections:
        computeConnectionThroughput(connection, resChannel.bandwidth)
        resChannel.throughput += connection.throughput
        
    return resChannel

def insertNextFree(sol, idConnection, band, variables):
    maxSpec = 0
    indexMaxSpec = 0
    indexMaxSlotSpec = 0
    auxBand = -1
    inserted = False
    
    for i in range(len(sol.slots)):
        for j in range(len(sol.slots[i].spectrums)):
            freeSpec = sol.slots[i].spectrums[j].maxFrequency - sol.slots[i].spectrums[j].usedFrequency
            if freeSpec >= band:
                sol.slots[i].spectrums[j].usedFrequency += band
                newChannel = Channel(band)
                newChannel = insertInChannel(newChannel, idConnection)
                sol.slots[i].spectrums[j].channels.append(newChannel)
                sol.throughput += newChannel.throughput
                inserted = True
                return band
            elif freeSpec > maxSpec:
                maxSpec = freeSpec
                indexMaxSpec = j
                indexMaxSlotSpec = i
                
                if maxSpec >= 80: auxBand = 80
                elif maxSpec >= 40: auxBand = 40
                else: auxBand = 20
    
    if not inserted and maxSpec > 0:
        newChannel = Channel(auxBand)
        newChannel = insertInChannel(newChannel, idConnection)
        sol.slots[indexMaxSlotSpec].spectrums[indexMaxSpec].usedFrequency += auxBand
        sol.slots[indexMaxSlotSpec].spectrums[indexMaxSpec].channels.append(newChannel)
        
        sol.throughput += newChannel.throughput
        
        if auxBand != band:
            idx = (idConnection * 2) + 1
            if newChannel.bandwidth == 20: variables[idx] = (0 + 0.25) / 2.0
            elif newChannel.bandwidth == 40: variables[idx] = (0.25 + 0.5) / 2.0
            elif newChannel.bandwidth == 80: variables[idx] = (0.5 + 0.75) / 2.0
            elif newChannel.bandwidth == 160: variables[idx] = (0.75 + 1.0) / 2.0
            
    return auxBand

def insertBestFree(sol, idConnection, band, variables):
    bestThroughput = sol.throughput
    inserted = False
    newChannel = Channel(band)
    tscBestThroughput = [-1, -1, -1]
    
    for t in range(len(sol.slots)):
        for s in range(len(sol.slots[t].spectrums)):
            for c in range(len(sol.slots[t].spectrums[s].channels)):
                toInsert = insertInChannel(sol.slots[t].spectrums[s].channels[c], idConnection)
                aux = sol.throughput - sol.slots[t].spectrums[s].channels[c].throughput + toInsert.throughput
                
                if aux > bestThroughput:
                    bestThroughput = aux
                    newChannel = toInsert
                    inserted = True
                    tscBestThroughput = [t, s, c]

    if inserted:
        sol.throughput = bestThroughput
        sol.slots[tscBestThroughput[0]].spectrums[tscBestThroughput[1]].channels[tscBestThroughput[2]] = newChannel
        
        if newChannel.bandwidth != band:
            idx = (idConnection * 2) + 1
            if newChannel.bandwidth == 20: variables[idx] = (0 + 0.25) / 2.0
            elif newChannel.bandwidth == 40: variables[idx] = (0.25 + 0.50) / 2.0
            elif newChannel.bandwidth == 80: variables[idx] = (0.5 + 0.75) / 2.0
            elif newChannel.bandwidth == 160: variables[idx] = (0.75 + 1.0) / 2.0

def decode(variables, n_links):
    n = len(variables)
    links = []
    
    for i in range(0, n, 2):
        k1 = variables[i]
        links.append((k1, i)) 
    
    links.sort() 
    
    slot0 = Slot()
    slot0.spectrums.append(Spectrum(160, 0))
    
    slot1 = Slot()
    slot1.spectrums.append(Spectrum(240, 0))
    
    slot2 = Slot()
    slot2.spectrums.append(Spectrum(100, 0))
    
    sol = SolutionObj([slot0, slot1, slot2], 0.0)
    
    totalSpectrum = 500.0
    totalUsed = 0.0
    
    i = 0
    while i < len(links) and totalUsed < totalSpectrum:
        conn_idx = links[i][1] // 2
        band = convertBand(variables[links[i][1] + 1])
        used = insertNextFree(sol, conn_idx, band, variables)
        if used != -1: totalUsed += used
        i += 1
        
    while i < len(links):
        conn_idx = links[i][1] // 2
        band = convertBand(variables[links[i][1] + 1])
        insertBestFree(sol, conn_idx, band, variables)
        i += 1
        
    return sol

def load_data_and_setup_globals(instance_path):
    global distanceMatrix, affectance, noise
    
    # LER COMO FLUXO DE TOKENS (Robusto igual ao C++)
    with open(instance_path, 'r') as f:
        # Lê o arquivo inteiro e divide por espaços em branco (espaço, tab, enter)
        tokens = f.read().split()
    
    # Criar um iterador para consumir os números um a um
    it = iter(tokens)
    
    try:
        # --- PARSE DO CABEÇALHO ---
        # Baseado na linha: 2048 3.0 0.0 1000.0 3 160 240 100
        
        # Ignora o primeiro valor (geralmente num variáveis ou ID)
        next(it) 
        
        alpha = float(next(it))       # 3.0
        noise_power = float(next(it)) # 0.0
        power = float(next(it))       # 1000.0
        
        # Consumir o restante do cabeçalho que não usamos agora 
        # (3 slots e os tamanhos 160, 240, 100)
        num_slots = int(float(next(it))) # 3
        for _ in range(num_slots):
            next(it) # Pula 160, 240, 100
            
        # --- PARSE DAS COORDENADAS ---
        coords = []
        while True:
            try:
                # Tenta pegar X e Y
                val_x = next(it)
                val_y = next(it)
                coords.append([float(val_x), float(val_y)])
            except StopIteration:
                break # Acabaram os números
        
        n_conns = len(coords) // 2
        
        # --- SETUP DAS MATRIZES ---
        distanceMatrix = [[0.0] * n_conns for _ in range(n_conns)]
        affectance = [[0.0] * n_conns for _ in range(n_conns)]
        noise = noise_power
        
        links_coords = []
        for i in range(n_conns):
            # O arquivo organiza como: Tx1, Rx1, Tx2, Rx2...
            tx = coords[2*i]     # [x, y]
            rx = coords[2*i+1]   # [x, y]
            
            links_coords.append({'tx': tx, 'rx': rx})
            
            # Distância Euclidiana Tx -> Rx (do próprio link)
            dist_ii = math.sqrt((tx[0]-rx[0])**2 + (tx[1]-rx[1])**2)
            distanceMatrix[i][i] = dist_ii
            
        # Calcular Matriz de Interferência (Affectance)
        for i in range(n_conns):     # Link Receptor (Vítima)
            for j in range(n_conns): # Link Transmissor (Agressor)
                tx_j = links_coords[j]['tx'] 
                rx_i = links_coords[i]['rx']
                
                # Distância do Tx do link J até o Rx do link I
                dist = math.sqrt((tx_j[0]-rx_i[0])**2 + (tx_j[1]-rx_i[1])**2)
                
                if dist < 0.001: dist = 0.001
                
                path_loss = 1.0 / (dist ** alpha)
                affectance[i][j] = path_loss * power
                
        return n_conns, links_coords, alpha, noise_power, power

    except StopIteration:
        print("Erro: O arquivo de instância terminou antes do esperado.")
        sys.exit(1)
    except Exception as e:
        print(f"Erro ao processar dados: {e}")
        sys.exit(1)
        
original_optimize = gp.Model.optimize

def optimize_with_fixing(self):
    global decoded_target_sol
    
    if decoded_target_sol:
        x_vars = {v.VarName: v for v in self.getVars() if "x_tc" in v.VarName}
        
        for t_idx, slot in enumerate(decoded_target_sol.slots):
            for spec in slot.spectrums:
                current_freq = 0.0
                for channel in spec.channels:
                    start_c_index = int(current_freq / 20)
                    num_slots_needed = int(channel.bandwidth / 20)
                    
                    for conn in channel.connections:
                        link_id = conn.id
                        tx_idx = link_id * 2
                        rx_idx = link_id * 2 + 1
                        
                        for offset in range(num_slots_needed):
                            c_idx = start_c_index + offset
                            var_name = f"x_tc[{tx_idx},{rx_idx},{t_idx},{c_idx}]"
                            
                            if var_name in x_vars:
                                x_vars[var_name].LB = 1.0
                                x_vars[var_name].UB = 1.0
                    
                    current_freq += channel.bandwidth

    self.computeIIS()
    original_optimize(self)

gp.Model.optimize = optimize_with_fixing

def main():
    instance_file = "../instances/instance_0.txt"
    solution_file = "../output/output_dp_5/2048/solution.txt"
    
    n_conns, links_data, alpha, N, P = load_data_and_setup_globals(instance_file)
    
    sol_lines = []
    with open(solution_file, 'r') as f:
        for line in f:
            if line.strip():
                sol_lines.append(list(map(float, line.split())))
    
    variables = sol_lines[0]
    
    global decoded_target_sol
    decoded_target_sol = decode(variables, n_conns)
    print(f"Decoded Throughput: {decoded_target_sol.throughput}")
    
    V = list(range(n_conns * 2))
    L = [(2*i, 2*i+1) for i in range(n_conns)]
    T = [0, 1, 2]
    
    max_bandwidth = 240
    C = list(range(int(max_bandwidth / 20)))
    
    instance_data = {
        "V": V,
        "L": L,
        "T": T,
        "C": C,
        "spectrums": [160, 240, 100],
        "R": [], 
        "lc": {20: 1, 40: 2, 80: 4, 160: 8},
        "rbs": {},
        "O": [],
        "P": {v: P for v in V},
        "d": {},
        "alpha": alpha,
        "N": N
    }
    
    for i in V:
        for j in V:
            pass 

    model.build_and_solve(instance_data)

if __name__ == "__main__":
    main()