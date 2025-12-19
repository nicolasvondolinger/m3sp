import pandas as pd
import os
import sys

# --- Configuração das Pastas ---
# Defina os caminhos exatos das suas pastas
folder_classic = 'output'           # A pasta "output" (Clássico)
folder_dp = 'output_dp'             # A pasta "output_dp" (DP Padrão)
folder_random = 'output_random_dp'  # A pasta "output_random_dp" (O que queremos que vença)

# Lista para facilitar a verificação de existência
folders_map = {
    'Classic': folder_classic,
    'DP': folder_dp,
    'Random': folder_random
}
# -------------------------------

def main():
    print(f"{'='*60}")
    print(f"Procurando a PRIMEIRA instância onde '{folder_random}' vence TODOS.")
    print(f"{'='*60}\n")

    # 1. Verificação de existência das pastas
    for label, path in folders_map.items():
        if not os.path.exists(path):
            print(f"ERRO FATAL: A pasta '{path}' ({label}) não foi encontrada.")
            return

    # 2. Identificação das Subpastas Comuns (10, 20, 50, etc.)
    try:
        common_subfolders = set(os.listdir(folder_classic))
        for path in [folder_dp, folder_random]:
            common_subfolders.intersection_update(os.listdir(path))
        
        # Filtra apenas numéricos e ordena (processar 10, depois 20, depois 50...)
        sorted_subfolders = sorted([d for d in common_subfolders if d.isdigit()], key=int)
        
        if not sorted_subfolders:
            print("Nenhuma subpasta numérica comum encontrada.")
            return

    except Exception as e:
        print(f"Erro ao listar subpastas: {e}")
        return

    # 3. Loop de Busca
    found = False

    for subfolder in sorted_subfolders:
        print(f"-> Verificando subpasta (tamanho): {subfolder}...")

        try:
            # Caminhos dos arquivos
            file_classic = os.path.join(folder_classic, subfolder, 'objectives.txt')
            file_dp = os.path.join(folder_dp, subfolder, 'objectives.txt')
            file_random = os.path.join(folder_random, subfolder, 'objectives.txt')

            # Leitura dos dados (Assumindo coluna 0 como o valor do Throughput)
            # header=None, sep=' ' (espaço)
            df_c = pd.read_csv(file_classic, sep=' ', header=None, usecols=[0], names=['Classic'])
            df_d = pd.read_csv(file_dp, sep=' ', header=None, usecols=[0], names=['DP'])
            df_r = pd.read_csv(file_random, sep=' ', header=None, usecols=[0], names=['Random'])

            # Concatena lado a lado
            df = pd.concat([df_c, df_d, df_r], axis=1)
            
            # Ajusta o índice para começar de 1 (Instância 1, 2, 3...)
            df.index = df.index + 1

            # --- A LÓGICA DE COMPARAÇÃO ---
            # Random deve ser ESTRITAMENTE MAIOR (>) que Classic E DP
            # Se quiser maior ou igual, troque > por >=
            condition = (df['Random'] > df['Classic']) & (df['Random'] > df['DP'])
            
            # Filtra apenas as linhas que satisfazem a condição
            winners = df[condition]

            if not winners.empty:
                # Pegamos a primeira ocorrência
                first_win = winners.iloc[0]
                instance_id = winners.index[0]

                print("\n" + "#"*60)
                print(f" ENCONTRADO NA SUBPASTA: {subfolder} | INSTÂNCIA: {instance_id}")
                print("#"*60)
                print(f"Detalhes dos Valores (Throughput):")
                print(f"  - {folder_random} (Vencedor): \t{first_win['Random']:.4f}")
                print(f"  - {folder_dp}:          \t\t{first_win['DP']:.4f}")
                print(f"  - {folder_classic}:           \t\t{first_win['Classic']:.4f}")
                print("-" * 30)
                
                diff_dp = first_win['Random'] - first_win['DP']
                diff_classic = first_win['Random'] - first_win['Classic']
                
                print(f"Diferença vs DP:      +{diff_dp:.4f}")
                print(f"Diferença vs Classic: +{diff_classic:.4f}")
                print("#"*60 + "\n")
                
                found = True
                break # Sai do loop de subpastas (para o script)

        except FileNotFoundError:
            print(f"   [!] Arquivo objectives.txt faltando em {subfolder}. Pulando.")
            continue
        except Exception as e:
            print(f"   [!] Erro ao ler dados em {subfolder}: {e}")
            continue

    if not found:
        print("\n:( Nenhuma instância foi encontrada onde o Random DP supera ambos os outros métodos.")

if __name__ == "__main__":
    main()