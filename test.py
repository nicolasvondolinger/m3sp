from gurobipy import Model, GRB, quicksum

# Função para carregar os dados do arquivo result.txt
def load_instance(file_path):
    with open(file_path, 'r') as file:
        lines = file.readlines()

    # Ler os parâmetros iniciais
    first_line = lines[0].strip().split(';')
    num_connections = int(first_line[0])
    alpha = float(first_line[1])
    noise = float(first_line[2])
    power_transmission_rate = float(first_line[3])
    spectrums = list(map(int, first_line[4].split()))

    # Ler posições dos emissores
    sender_positions = []
    for i in range(1, num_connections + 1):
        sender_positions.append(tuple(map(float, lines[i].strip().split())))

    # Ler posições dos receptores
    receiver_positions = []
    for i in range(num_connections + 1, 2 * num_connections + 1):
        receiver_positions.append(tuple(map(float, lines[i].strip().split())))

    # Ler valores de gamma
    gamma_values = list(map(float, lines[2 * num_connections + 1:3 * num_connections + 1]))

    # Ler taxas de dados e valores de SINR
    data_rates = list(map(float, lines[3 * num_connections + 1:4 * num_connections + 1]))
    sinr_values = list(map(float, lines[4 * num_connections + 1:5 * num_connections + 1]))

    return {
        'num_connections': num_connections,
        'alpha': alpha,
        'noise': noise,
        'power_transmission_rate': power_transmission_rate,
        'spectrums': spectrums,
        'sender_positions': sender_positions,
        'receiver_positions': receiver_positions,
        'gamma_values': gamma_values,
        'data_rates': data_rates,
        'sinr_values': sinr_values,
    }

# Carregar a instância do arquivo result.txt
instance_path = "../master-thesis/instances/MD-VRBSP_U_10_1.txt"
data = load_instance(instance_path)

# Inicializar variáveis do problema
num_connections = data['num_connections']
alpha = data['alpha']
noise = data['noise']
power_transmission_rate = data['power_transmission_rate']
spectrums = data['spectrums']
sender_positions = data['sender_positions']
receiver_positions = data['receiver_positions']
gamma_values = data['gamma_values']
data_rates = data['data_rates']
sinr_values = data['sinr_values']

# Definir os conjuntos e parâmetros com base nos dados carregados
V = list(range(num_connections))  # Nós
L = [(i, j) for i in range(num_connections) for j in range(num_connections) if i != j]  # Links possíveis
B = spectrums  # Bandas disponíveis
T = list(range(10))  # Intervalos de tempo (ajuste conforme necessário)
C = list(range(len(B)))  # Canais
R = {(b, s): data_rates[s] for b in B for s in range(len(data_rates))}  # Mapear bandas e velocidades
O = [(c, c_) for c in C for c_ in C if c != c_]  # Canais que se sobrepõem
lc = {c: B[c] for c in C}  # Largura de banda por canal
Pij = {(i, j): power_transmission_rate for (i, j) in L}  # Potência do link
dij = {(i, j): ((sender_positions[i][0] - receiver_positions[j][0])**2 +
                (sender_positions[i][1] - receiver_positions[j][1])**2)**0.5
       for (i, j) in L}  # Distância entre os nós

# Criação do modelo
model = Model("M3SP")

# Variáveis de decisão
x = model.addVars(L, T, C, vtype=GRB.BINARY, name="x")  # Link em intervalo e canal
y = model.addVars(L, R.keys(), vtype=GRB.BINARY, name="y")  # Velocidade do link

# Variáveis auxiliares
I = model.addVars(L, vtype=GRB.CONTINUOUS, name="I")  # Interferência
SINR = model.addVars(L, vtype=GRB.CONTINUOUS, name="SINR")  # SINR

# Função objetivo: Maximizar throughput médio
model.setObjective(
    quicksum(R[b, s] * y[i, j, b, s] for (i, j) in L for (b, s) in R.keys()) / len(T), GRB.MAXIMIZE
)

# Restrições
# (2) Cada link transmite em um único intervalo de tempo e canal
model.addConstrs((quicksum(x[i, j, t, c] for t in T for c in C) == 1 for (i, j) in L), name="one_slot_channel")

# (3) Cada link transmite em uma única velocidade
model.addConstrs((quicksum(y[i, j, b, s] for (b, s) in R.keys()) == 1 for (i, j) in L), name="one_speed")

# (4) Cálculo da interferência
model.addConstrs((
    I[i, j] == quicksum(
        Pij[i_, j_] / (dij[i_, j_] ** alpha) * quicksum(x[i_, j_, t, c_] * x[i, j, t, c] for t in T for (c, c_) in O)
        for (i_, j_) in L if (i_, j_) != (i, j)
    )
    for (i, j) in L
), name="interference")

# (5) Cálculo do SINR
model.addConstrs((
    SINR[i, j] == Pij[i, j] / (dij[i, j] ** alpha) / (I[i, j] + noise)
    for (i, j) in L
), name="SINR")

# Resolver o modelo
model.optimize()

# Exibir solução
if model.status == GRB.OPTIMAL:
    print("Solução ótima encontrada!")
    for (i, j, t, c) in x.keys():
        if x[i, j, t, c].x > 0.5:
            print(f"Link {(i, j)} atribuído a intervalo {t} no canal {c}.")
else:
    print("Nenhuma solução ótima encontrada.")
