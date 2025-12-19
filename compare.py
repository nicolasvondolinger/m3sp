import pandas as pd
import matplotlib.pyplot as plt
import seaborn as sns
import os
import re
import shutil
import numpy as np

# --- Configurações Globais ---
ROOT_OUTPUT_DIR = "output"
PLOTS_ROOT_DIR = "comparison_plots_dynamic"

# Mapeamento de Labels
TYPE_MAP = {
    'output_dp': 'BRKGA_DP',          
    'output': 'BRKGA_Classic',        
    'output_random_dp': 'BRKGA_Random', 
    'output_fixed_dp': 'BRKGA_Fixed'
}
# -----------------------------

def parse_output_folders(root_dir):
    """
    Escaneia o diretório root e agrupa pastas pelo sufixo numérico.
    """
    groups = {}
    
    if not os.path.exists(root_dir):
        print(f"Erro: Diretório '{root_dir}' não encontrado.")
        return {}

    pattern = re.compile(r'(.+)_(\d+)$')

    for item in os.listdir(root_dir):
        full_path = os.path.join(root_dir, item)
        if os.path.isdir(full_path):
            match = pattern.match(item)
            if match:
                base_name = match.group(1) 
                suffix = match.group(2)    
                
                label = TYPE_MAP.get(base_name, base_name)
                
                if suffix not in groups:
                    groups[suffix] = []
                
                groups[suffix].append({
                    'label': label,
                    'path': full_path,
                    'type': base_name 
                })

    return groups

def analyze_group(suffix, folders_config):
    """
    Processa um grupo específico (ex: sufixo 25), gera gráficos centralizados e estatísticas.
    """
    print(f"\n{'#'*80}")
    print(f" ANALISANDO GRUPO: PE = {suffix}".center(80))
    print(f"{'#'*80}")

    # 1. Definir Baseline e Comparação
    baseline_cfg = next((f for f in folders_config if f['type'] == 'output_dp'), None)
    
    if not baseline_cfg:
        baseline_cfg = folders_config[0]
        print(f"Aviso: 'output_dp' não encontrado. Usando {baseline_cfg['label']} como base.")

    comparison_cfgs = [f for f in folders_config if f != baseline_cfg]
    all_configs = [baseline_cfg] + comparison_cfgs
    all_labels = [c['label'] for c in all_configs]
    
    print(f"-> Baseline: {baseline_cfg['label']}")
    print(f"-> Comparando contra: {', '.join([c['label'] for c in comparison_cfgs])}")

    # 2. Configurar Pasta de Saída CENTRALIZADA para o Grupo
    group_plot_dir = os.path.join(PLOTS_ROOT_DIR, f"group_{suffix}")
    
    # Limpa a pasta do grupo para começar do zero
    if os.path.exists(group_plot_dir):
        shutil.rmtree(group_plot_dir)
    os.makedirs(group_plot_dir)
    
    print(f"-> Pasta de Plots Centralizada: {group_plot_dir}")

    # 3. Encontrar Instâncias Comuns
    try:
        common_subfolders = set(os.listdir(baseline_cfg['path']))
        for cfg in comparison_cfgs:
            common_subfolders.intersection_update(os.listdir(cfg['path']))
        
        instances = sorted([d for d in common_subfolders if d.isdigit()], key=int)
        
        if not instances:
            print(f" -> Nenhuma instância comum encontrada no grupo {suffix}. Pulando.")
            return

    except Exception as e:
        print(f" -> Erro ao ler subpastas no grupo {suffix}: {e}")
        return

    # 4. Loop por Instância
    for instance in instances:
        print(f"\n{'-'*60}")
        print(f" Processando Instância: {instance} (Grupo {suffix})")
        print(f"{'-'*60}")
        
        try:
            # --- Carregar Dados ---
            all_dfs = []
            obj_cols = []
            time_cols = []

            for cfg in all_configs:
                label = cfg['label']
                path = cfg['path']
                
                obj_col = f'objective_{label}'
                time_col = f'time_{label}'
                obj_cols.append(obj_col)
                time_cols.append(time_col)
                
                f_obj = os.path.join(path, instance, 'objectives.txt')
                f_time = os.path.join(path, instance, 'time.txt')
                
                if not os.path.exists(f_obj) or not os.path.exists(f_time):
                    raise FileNotFoundError(f"Arquivos faltando em {label}")

                df_o = pd.read_csv(f_obj, sep=' ', header=None, usecols=[0], names=[obj_col])
                df_t = pd.read_csv(f_time, header=None, names=[time_col])
                all_dfs.extend([df_o, df_t])

            df = pd.concat(all_dfs, axis=1)
            df['instance'] = df.index + 1
            epsilon = 1e-9

            # --- Estatísticas no Terminal ---
            
            # Vencedores
            df['winner_obj'] = df[obj_cols].apply(
                lambda row: 'Empate' if (row == row.max()).sum() > 1 else row.idxmax(), axis=1
            )
            df['winner_time'] = df[time_cols].apply(
                lambda row: 'Empate' if (row == row.min()).sum() > 1 else row.idxmin(), axis=1
            )

            print("\n[OBJETIVO] Contagem de vitórias (maior é melhor):")
            print(df['winner_obj'].str.replace('objective_', '').value_counts())
            
            print("\n[TEMPO] Contagem de vitórias (menor é melhor):")
            print(df['winner_time'].str.replace('time_', '').value_counts())
            
            print("\n--- Estatísticas (Valores da Função Objetivo) ---")
            print(df[obj_cols].describe().to_string())
            
            print("\n--- Estatísticas (Tempo de Execução) ---")
            print(df[time_cols].describe().to_string())

            # --- Gráficos (Salvos na pasta do Grupo com prefixo) ---
            
            base_obj = obj_cols[0]
            base_time = time_cols[0]
            
            sns.set_style("whitegrid")
            palette = sns.color_palette("bright", len(all_labels))
            palette_map = dict(zip(all_labels, palette))

            # Prefixo para identificar a instância dentro da pasta central
            file_prefix = f"Inst_{instance}_"

            # 1. Barplot Objetivo
            plt.figure(figsize=(12, 6))
            df_melt = df.melt(id_vars='instance', value_vars=obj_cols, var_name='Metodo', value_name='Valor')
            df_melt['Metodo'] = df_melt['Metodo'].str.replace('objective_', '')
            sns.barplot(x='instance', y='Valor', hue='Metodo', data=df_melt, palette=palette_map)
            plt.title(f'Objetivo - Grupo {suffix} - Instância {instance}')
            plt.tight_layout()
            # Salva direto na pasta do grupo
            plt.savefig(os.path.join(group_plot_dir, f'{file_prefix}01_objective_bar.png'))
            plt.close()

            # 2. Boxplot Objetivo
            plt.figure(figsize=(8, 6))
            sns.boxplot(data=df[obj_cols], palette=palette)
            plt.xticks(range(len(all_labels)), all_labels, rotation=15)
            plt.title(f'Distribuição Objetivo - Grupo {suffix} - Instância {instance}')
            plt.tight_layout()
            plt.savefig(os.path.join(group_plot_dir, f'{file_prefix}02_objective_box.png'))
            plt.close()

            # 3. Scatters
            for cfg in comparison_cfgs:
                lbl = cfg['label']
                c_obj = f'objective_{lbl}'
                
                winner_hue = df.apply(lambda r: lbl if r[c_obj] > r[base_obj] else (baseline_cfg['label'] if r[base_obj] > r[c_obj] else 'Empate'), axis=1)

                plt.figure(figsize=(7, 7))
                pal_scat = {lbl: palette_map[lbl], baseline_cfg['label']: palette_map[baseline_cfg['label']], 'Empate': 'gray'}
                sns.scatterplot(x=base_obj, y=c_obj, data=df, hue=winner_hue, palette=pal_scat, s=60)
                
                min_v = min(df[base_obj].min(), df[c_obj].min())
                max_v = max(df[base_obj].max(), df[c_obj].max())
                plt.plot([min_v, max_v], [min_v, max_v], 'k--', alpha=0.5, label='Empate')
                
                plt.xlabel(baseline_cfg['label'])
                plt.ylabel(lbl)
                plt.title(f'{lbl} vs {baseline_cfg["label"]} (Grupo {suffix} - {instance})')
                plt.tight_layout()
                plt.savefig(os.path.join(group_plot_dir, f'{file_prefix}03_scatter_{lbl}.png'))
                plt.close()

            # 4. Barplot Tempo
            plt.figure(figsize=(12, 6))
            df_melt_t = df.melt(id_vars='instance', value_vars=time_cols, var_name='Metodo', value_name='Tempo(s)')
            df_melt_t['Metodo'] = df_melt_t['Metodo'].str.replace('time_', '')
            sns.barplot(x='instance', y='Tempo(s)', hue='Metodo', data=df_melt_t, palette=palette_map)
            plt.title(f'Tempo - Grupo {suffix} - Instância {instance}')
            plt.tight_layout()
            plt.savefig(os.path.join(group_plot_dir, f'{file_prefix}04_time_bar.png'))
            plt.close()

            print(f"   -> Gráficos gerados para instância {instance}")

        except Exception as e:
            print(f"   [ERRO] Falha na instância {instance}: {e}")

def main():
    if not os.path.exists(PLOTS_ROOT_DIR):
        os.makedirs(PLOTS_ROOT_DIR)

    print(f"Escaneando pasta '{ROOT_OUTPUT_DIR}' por grupos de teste...")
    groups = parse_output_folders(ROOT_OUTPUT_DIR)
    
    if not groups:
        print("Nenhuma pasta válida encontrada em 'output/'. Esperado formato: nome_NUMERO")
        return

    print(f"Grupos encontrados: {list(groups.keys())}")

    # Ordena grupos (5, 25...)
    for suffix in sorted(groups.keys(), key=lambda x: int(x) if x.isdigit() else 999999):
        analyze_group(suffix, groups[suffix])

    print(f"\n{'='*30}\n Processo Total Concluído \n{'='*30}")

if __name__ == "__main__":
    main()