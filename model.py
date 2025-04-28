import gurobipy as gp
from gurobipy import GRB

def build_and_solve(instance_data):
    # Extract parameters
    V = instance_data["V"]
    L = instance_data["L"]
    T = instance_data["T"]
    C = instance_data["C"]
    B = [20, 40, 80, 160]  # Standard bandwidths per IEEE 802.11ac
    R = [(b, s) for b in B for s in instance_data["sinrs"]]  # All (b,s) pairs
    lc = instance_data["lc"]  # Channel bandwidth mapping
    rbs = instance_data["rbs"]  # Transmission speeds
    O = instance_data["O"]  # Overlapping channel pairs
    P = instance_data["P"]  # Transmission powersV = instance_data["V"]
    L = instance_data["L"]
    T = instance_data["T"]
    C = instance_data["C"]
    R = instance_data["R"]
    lc = instance_data["lc"]
    rbs = instance_data["rbs"]
    O = instance_data["O"]
    P = instance_data["P"]
    d = instance_data["d"]
    alpha = instance_data["alpha"]
    N = instance_data["N"]
    d = instance_data["d"]  # Distances
    alpha = instance_data["alpha"]  # Path loss exponent
    N = instance_data["N"]  # Noise power

    model = gp.Model("m3sp")

    # Define variables
    x_tc = model.addVars([(i, j, t, c) for (i, j) in L for t in T for c in C], vtype=GRB.BINARY, name="x_tc")
    y_bs = model.addVars([(i, j, b, s) for (i,j) in L for (b,s) in R], vtype=GRB.BINARY, name="y_bs")
    i_ij = model.addVars(L, vtype=GRB.CONTINUOUS, name="i_ij")
    sinr_ij = model.addVars(L, vtype=GRB.CONTINUOUS, name="sinr_ij")

    # Objective function
    model.setObjective(
        (1/len(T)) * gp.quicksum(
            rbs[(b,s)] * y_bs[i, j, b, s] 
            for (i, j) in L 
            for (b, s) in R
        ), 
        GRB.MAXIMIZE
    )

    # Constraints
    for (i, j) in L:
        model.addConstr(
            gp.quicksum(x_tc[i,j,t,c] for t in T for c in C) == 1,
            name=f"one_time_channel_{i}_{j}"
        )

    for (i, j) in L:
        model.addConstr(
            gp.quicksum(y_bs[i, j, b, s] for (b, s) in R) == 1,
            name=f"one_speed_{i}_{j}"
        )

    for (i, j) in L:
        interference = gp.quicksum(
            (P[i2, j2] / (d[i2, j2]**alpha)) * 
            gp.quicksum(
                x_tc[i,j,t,c] * x_tc[i2,j2,t,c2]
                for t in T
                for (c,c2) in O
            )
            for (i2, j2) in L if (i2, j2) != (i, j)
        )
        model.addConstr(i_ij[i,j] == interference, name=f"interference_{i}_{j}")

    for (i, j) in L:
        model.addConstr(
            sinr_ij[i, j] == (P[i, j] / (d[i,j]**alpha) / (i_ij[i,j] + N)),
            name=f"sinr_{i}_{j}"
        )

    for (i, j) in L:
        for (b, s) in R:
            model.addConstr(
                y_bs[i, j, b, s] <= gp.quicksum(x_tc[i,j,t,c] for t in T for c in C if lc[c] == b),
                name=f"speed_bandwidth_{i}_{j}_{b}_{s}"
            )

    for i in V:
        for t in T:
            model.addConstr(
                gp.quicksum(x_tc[i,j,t,c] for (i2, j) in L if i2 == i for c in C) +
                gp.quicksum(x_tc[j,i2,t,c] for (j, i2) in L if i2 == i for c in C) <= 1,
                name=f"one_link_per_device_{i}_time_{t}"
            )

    model.optimize()
    model.write("model.lp")

    if model.status == GRB.OPTIMAL:
        print("\n--- Optimal Solution Found ---\n")
        for (i, j, t, c) in x_tc.keys():
            if x_tc[i,j,t,c].X > 0.5:
                print(f"Link ({i},{j}) active at time {t} on channel {c}")
        for (i, j, b, s) in y_bs.keys():
            if y_bs[i,j,b,s].X > 0.5:
                print(f"Link ({i},{j}) transmits at speed {rbs[(b,s)]} Mbps with bandwidth {b} MHz and SINR ≥ {s} dBm")
    else:
        print("\nNo optimal solution found.")

    return model