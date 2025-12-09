import pandas as pd
import matplotlib.pyplot as plt
import seaborn as sns
import os
import numpy as np
import shutil

# --- Configuração das Pastas ---
# Defina a pasta base. O script SEMPRE a usará como referência.
baseline_config = {
    'label': 'BRKGA_Padrão', 
    'path': 'output_dp'
}

# Adicione outras pastas (o "array") para comparar contra a base.
# O script processará todas que encontrar.
comparison_folders_list = [
    {'label': 'BRKGA_DP', 'path': 'output_dp'},
    {'label': 'BRKGA_RANDOM_DP', 'path': 'output_random_dp'}
]

output_folder_plots = 'comparison_plots_dynamic'
# --- Fim da Configuração ---


# Cria a pasta de saída principal para os gráficos
os.makedirs(output_folder_plots, exist_ok=True)

# --- 1. Verificação das Pastas ---
# Verifica se a pasta base existe
if not os.path.exists(baseline_config['path']):
    print(f"Erro fatal: A pasta base '{baseline_config['path']}' não foi encontrada. O script não pode continuar.")
    exit()

# Filtra a lista de comparação para incluir apenas pastas que existem
existing_comparison_folders = []
for folder_config in comparison_folders_list:
    if os.path.exists(folder_config['path']):
        existing_comparison_folders.append(folder_config)
    else:
        print(f"Aviso: Pasta de comparação '{folder_config['path']}' não encontrada. Será ignorada.")

if not existing_comparison_folders:
    print("Erro: Nenhuma das pastas de comparação foi encontrada. O script não tem nada para comparar com a base.")
    exit()

# Lista final de todas as pastas que serão processadas
all_existing_configs = [baseline_config] + existing_comparison_folders
all_labels = [f['label'] for f in all_existing_configs]

print(f"Pastas que serão comparadas: {', '.join(all_labels)}")

# --- 2. Identificação das Subpastas Comuns ---
try:
    # Pega as subpastas da pasta base
    base_subfolders = set(os.listdir(baseline_config['path']))
    common_subfolders_set = base_subfolders
    
    # Faz a interseção com as subpastas das outras pastas encontradas
    for folder_config in existing_comparison_folders:
        comp_subfolders = set(os.listdir(folder_config['path']))
        common_subfolders_set.intersection_update(comp_subfolders)
        
    # Filtra para manter apenas diretórios numéricos e os ordena
    common_subfolders = sorted([d for d in common_subfolders_set if d.isdigit()], key=int)

    if not common_subfolders:
        print(f"Nenhuma subpasta numérica em comum encontrada entre: {', '.join(all_labels)}")
        exit()

    print(f"Subpastas comuns encontradas para comparação: {', '.join(common_subfolders)}")

except FileNotFoundError as e:
    print(f"Erro: A pasta '{e.filename}' não foi encontrada durante a varredura de subpastas.")
    print("Certifique-se de que as pastas configuradas existem.")
    exit()


# --- 3. Loop Principal para Análise e Visualização ---
for subfolder in common_subfolders:
    print(f"\n{'='*25} Processando Subpasta: {subfolder} {'='*25}")

    # Define a pasta de saída para os gráficos desta subpasta
    current_plot_folder = os.path.join(output_folder_plots, subfolder)
    if os.path.exists(current_plot_folder):
        shutil.rmtree(current_plot_folder)
    os.makedirs(current_plot_folder)

    try:
        # --- Leitura e Preparação dos Dados (Dinâmico) ---
        all_dataframes = []
        obj_cols = []  # Nomes das colunas de objetivo (ex: 'objective_BRKGA_Padrão')
        time_cols = [] # Nomes das colunas de tempo (ex: 'time_BRKGA_Padrão')

        # Carrega os dados de todas as pastas
        for folder_config in all_existing_configs:
            label = folder_config['label']
            path = folder_config['path']
            
            obj_col_name = f'objective_{label}'
            time_col_name = f'time_{label}'
            obj_cols.append(obj_col_name)
            time_cols.append(time_col_name)

            obj_file = os.path.join(path, subfolder, 'objectives.txt')
            time_file = os.path.join(path, subfolder, 'time.txt')

            df_obj = pd.read_csv(obj_file, sep=' ', header=None, usecols=[0], names=[obj_col_name])
            df_time = pd.read_csv(time_file, header=None, names=[time_col_name])
            
            all_dataframes.extend([df_obj, df_time])

        # Combinar todos os dados em um único DataFrame
        df_comparison = pd.concat(all_dataframes, axis=1)
        df_comparison['instance'] = df_comparison.index + 1
        epsilon = 1e-9
        
        # --- Análise Comparativa (Dinâmica) ---
        base_obj_col = obj_cols[0]  # Coluna de objetivo da baseline
        base_time_col = time_cols[0] # Coluna de tempo da baseline

        # ===================================================================
        # == CORREÇÃO AQUI ==
        # ===================================================================
        
        # Vencedores (maior objetivo é melhor)
        # Verifica se mais de um tem o valor máximo (Empate)
        df_comparison['winner_obj'] = df_comparison[obj_cols].apply(
            lambda row: 'Empate' if (row == row.max()).sum() > 1 else row.idxmax(), 
            axis=1
        )
        
        # Vencedores (menor tempo é melhor)
        # Verifica se mais de um tem o valor mínimo (Empate)
        df_comparison['winner_time'] = df_comparison[time_cols].apply(
            lambda row: 'Empate' if (row == row.min()).sum() > 1 else row.idxmin(),
            axis=1
        )
        
        # ===================================================================
        # == FIM DA CORREÇÃO ==
        # ===================================================================

        # Fatores (sempre contra a baseline)
        factor_imp_cols = []
        factor_speed_cols = []
        
        for comp_config in existing_comparison_folders:
            comp_label = comp_config['label']
            comp_obj_col = f'objective_{comp_label}'
            comp_time_col = f'time_{comp_label}'
            
            # Fator de Melhoria (Objetivo)
            imp_factor_col_name = f'improvement_factor_vs_{comp_label}'
            factor_imp_cols.append(imp_factor_col_name)
            df_comparison[imp_factor_col_name] = df_comparison[comp_obj_col] / (df_comparison[base_obj_col] + epsilon)
            
            # Fator de Speedup (Tempo)
            speed_factor_col_name = f'speedup_factor_vs_{comp_label}'
            factor_speed_cols.append(speed_factor_col_name)
            df_comparison[speed_factor_col_name] = df_comparison[base_time_col] / (df_comparison[comp_time_col] + epsilon)

        # --- Exibindo Resultados no Console ---
        print(f"--- Resumo da Comparação para a subpasta '{subfolder}' ---")
        print("\n[OBJETIVO] Contagem de vitórias (maior é melhor):")
        # Renomeia para melhor leitura no console
        print(df_comparison['winner_obj'].str.replace('objective_', '').value_counts())
        print("\n[TEMPO] Contagem de vitórias (menor é melhor):")
        print(df_comparison['winner_time'].str.replace('time_', '').value_counts())
        
        print("\n--- Estatísticas Descritivas (Valores da Função Objetivo) ---")
        print(df_comparison[obj_cols].describe())
        print("\n--- Estatísticas Descritivas (Tempo de Execução em segundos) ---")
        print(df_comparison[time_cols].describe())
        
        print("\n--- Tabela Comparativa (Primeiras 15 instâncias) ---")
        cols_to_print = ['instance'] + obj_cols + time_cols
        print(df_comparison[cols_to_print].head(15).to_string())

        # --- Gerando Gráficos (Dinâmico) ---
        sns.set_style("whitegrid")
        
        # Paleta de cores dinâmica
        palette = sns.color_palette("bright", len(all_labels))
        palette_map = {label: color for label, color in zip(all_labels, palette)}
        palette_map_factor_imp = {f.replace('improvement_factor_vs_', ''): palette_map[f.replace('improvement_factor_vs_', '')] for f in factor_imp_cols}
        palette_map_factor_speed = {f.replace('speedup_factor_vs_', ''): palette_map[f.replace('speedup_factor_vs_', '')] for f in factor_speed_cols}

        # ===================================================================
        # GRÁFICOS PARA OS VALORES DA FUNÇÃO OBJETIVO
        # ===================================================================

        # 1. Gráfico de Barras Comparativo por Instância (Objetivo)
        plt.figure(figsize=(18, 8))
        df_melted_obj = df_comparison.melt(id_vars='instance', value_vars=obj_cols,
                                           var_name='Abordagem', value_name='Valor Objetivo')
        df_melted_obj['Abordagem'] = df_melted_obj['Abordagem'].str.replace('objective_', '') # Limpa o nome
        sns.barplot(x='instance', y='Valor Objetivo', hue='Abordagem', data=df_melted_obj, palette=palette_map)
        plt.title(f'Comparação do Valor Objetivo por Instância (Subpasta: {subfolder})', fontsize=16)
        plt.xlabel('Instância', fontsize=12)
        plt.ylabel('Valor da Função Objetivo', fontsize=12)
        plt.xticks(rotation=45, ha='right')
        plt.tight_layout()
        plt.savefig(os.path.join(current_plot_folder, '01_objective_comparison_bar.png'))
        plt.close()

        # 2. Boxplot para Comparação da Distribuição (Objetivo)
        plt.figure(figsize=(10, 7))
        sns.boxplot(data=df_comparison[obj_cols])
        plt.title(f'Distribuição dos Valores da Função Objetivo (Subpasta: {subfolder})', fontsize=16)
        plt.ylabel('Valor da Função Objetivo', fontsize=12)
        plt.xlabel('Abordagem', fontsize=12)
        plt.xticks(ticks=range(len(all_labels)), labels=all_labels)
        plt.savefig(os.path.join(current_plot_folder, '02_objective_boxplot.png'))
        plt.close()

        # 3. Gráficos de Dispersão (Loop, 1 para cada comparação vs. baseline)
        for comp_config in existing_comparison_folders:
            comp_label = comp_config['label']
            comp_obj_col = f'objective_{comp_label}'
            
            # Define o vencedor apenas para este par (baseline vs. comparado)
            # Esta lógica já estava correta e não precisou de mudança
            winner_hue = df_comparison.apply(
                lambda r: comp_label if r[comp_obj_col] > r[base_obj_col] else (baseline_config['label'] if r[base_obj_col] > r[comp_obj_col] else 'Empate'),
                axis=1
            )
            
            plt.figure(figsize=(10, 10))
            sns.scatterplot(x=base_obj_col, y=comp_obj_col, data=df_comparison, hue=winner_hue, s=60, 
                            palette={comp_label: palette_map[comp_label], baseline_config['label']: palette_map[baseline_config['label']], 'Empate': 'gray'})
            
            max_val = max(df_comparison[base_obj_col].max(), df_comparison[comp_obj_col].max())
            min_val = min(df_comparison[base_obj_col].min(), df_comparison[comp_obj_col].min())
            plt.plot([min_val, max_val], [min_val, max_val], 'k--', lw=2, label='y=x (Empate)')
            
            plt.title(f'{comp_label} vs. {baseline_config["label"]} (Subpasta: {subfolder})', fontsize=16)
            plt.xlabel(f'Valor Objetivo ({baseline_config["label"]})', fontsize=12)
            plt.ylabel(f'Valor Objetivo ({comp_label})', fontsize=12)
            plt.legend(title='Melhor Resultado')
            plt.axis('equal')
            plt.grid(True)
            plt.savefig(os.path.join(current_plot_folder, f'03_scatter_{comp_label}_vs_{baseline_config["label"]}.png'))
            plt.close()

        # 4. Gráfico de Barras do Fator de Melhoria (Agrupado)
        if factor_imp_cols: # Só plota se houver colunas de fator
            plt.figure(figsize=(18, 8))
            df_melted_imp = df_comparison.melt(id_vars='instance', value_vars=factor_imp_cols,
                                               var_name='Comparação', value_name='Fator de Melhoria')
            df_melted_imp['Comparação'] = df_melted_imp['Comparação'].str.replace('improvement_factor_vs_', '') # Limpa o nome
            sns.barplot(x='instance', y='Fator de Melhoria', hue='Comparação', data=df_melted_imp, palette=palette_map_factor_imp)
            plt.axhline(y=1, color='r', linestyle='--', label='Empate (Fator = 1)')
            plt.title(f'Fator de Melhoria do Objetivo (vs. {baseline_config["label"]}) por Instância (Subpasta: {subfolder})', fontsize=16)
            plt.xlabel('Instância', fontsize=12)
            plt.ylabel(f'Fator de Melhoria (Valor / {base_obj_col})', fontsize=12)
            plt.xticks(rotation=45, ha='right')
            plt.legend(title='Comparação')
            plt.tight_layout()
            plt.savefig(os.path.join(current_plot_folder, '04_objective_improvement_factor_grouped.png'))
            plt.close()

        # ===================================================================
        # GRÁFICOS PARA O TEMPO DE EXECUÇÃO
        # ===================================================================

        # 5. Gráfico de Barras Comparativo por Instância (Tempo)
        plt.figure(figsize=(18, 8))
        df_melted_time = df_comparison.melt(id_vars='instance', value_vars=time_cols,
                                            var_name='Abordagem', value_name='Tempo (s)')
        df_melted_time['Abordagem'] = df_melted_time['Abordagem'].str.replace('time_', '') # Limpa o nome
        sns.barplot(x='instance', y='Tempo (s)', hue='Abordagem', data=df_melted_time, palette=palette_map)
        plt.title(f'Comparação do Tempo de Execução por Instância (Subpasta: {subfolder})', fontsize=16)
        plt.xlabel('Instância', fontsize=12)
        plt.ylabel('Tempo de Execução (segundos)', fontsize=12)
        plt.xticks(rotation=45, ha='right')
        plt.tight_layout()
        plt.savefig(os.path.join(current_plot_folder, '05_time_comparison_bar.png'))
        plt.close()

        # 6. Boxplot para Comparação da Distribuição (Tempo)
        plt.figure(figsize=(10, 7))
        sns.boxplot(data=df_comparison[time_cols])
        plt.title(f'Distribuição dos Tempos de Execução (Subpasta: {subfolder})', fontsize=16)
        plt.ylabel('Tempo de Execução (segundos)', fontsize=12)
        plt.xlabel('Abordagem', fontsize=12)
        plt.xticks(ticks=range(len(all_labels)), labels=all_labels)
        plt.savefig(os.path.join(current_plot_folder, '06_time_boxplot.png'))
        plt.close()
        
        # 7. Gráfico de Barras do Fator de Speedup (Agrupado)
        if factor_speed_cols: # Só plota se houver colunas de fator
            plt.figure(figsize=(18, 8))
            df_melted_speed = df_comparison.melt(id_vars='instance', value_vars=factor_speed_cols,
                                                 var_name='Comparação', value_name='Fator de Speedup')
            df_melted_speed['Comparação'] = df_melted_speed['Comparação'].str.replace('speedup_factor_vs_', '') # Limpa o nome
            sns.barplot(x='instance', y='Fator de Speedup', hue='Comparação', data=df_melted_speed, palette=palette_map_factor_speed)
            plt.axhline(y=1, color='r', linestyle='--', label='Empate (Speedup = 1)')
            plt.title(f'Fator de Speedup (vs. {baseline_config["label"]}) por Instância (Subpasta: {subfolder})', fontsize=16)
            plt.xlabel('Instância', fontsize=12)
            plt.ylabel(f'Fator de Speedup ({base_time_col} / Tempo)', fontsize=12)
            plt.xticks(rotation=45, ha='right')
            plt.legend(title='Comparação')
            plt.tight_layout()
            plt.savefig(os.path.join(current_plot_folder, '07_time_speedup_factor_grouped.png'))
            plt.close()

        print(f"-> Gráficos para a subpasta '{subfolder}' salvos com sucesso em: '{current_plot_folder}'")

    except FileNotFoundError as e:
        print(f"!! Erro ao processar a subpasta '{subfolder}': Arquivo não encontrado -> {e.filename}.")
        print("   Verifique se os arquivos 'objectives.txt' e 'time.txt' existem nesta subpasta (em todas as pastas configuradas). Pulando para a próxima.")
    except Exception as e:
        print(f"!! Ocorreu um erro inesperado ao processar a subpasta '{subfolder}': {e}")

print(f"\n{'='*20} Processo de Geração de Gráficos Concluído {'='*20}\n")